// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_handle_impl.h"
#include "base/macros.h"
#include "base/optional.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_side_navigation_test_utils.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace content {

// Test version of a NavigationThrottle that will execute a callback when
// called.
class DeletingNavigationThrottle : public NavigationThrottle {
 public:
  DeletingNavigationThrottle(NavigationHandle* handle,
                             const base::RepeatingClosure& deletion_callback)
      : NavigationThrottle(handle), deletion_callback_(deletion_callback) {}
  ~DeletingNavigationThrottle() override {}

  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillFailRequest() override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  const char* GetNameForLogging() override {
    return "DeletingNavigationThrottle";
  }

 private:
  base::RepeatingClosure deletion_callback_;
};

class NavigationHandleImplTest : public RenderViewHostImplTestHarness {
 public:
  NavigationHandleImplTest()
      : was_callback_called_(false),
        callback_result_(NavigationThrottle::DEFER) {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    CreateNavigationHandle();
    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
  }

  void TearDown() override {
    // Release the |test_handle_| before destroying the WebContents, to match
    // the WebContentsObserverSanityChecker expectations.
    test_handle_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  void Resume() { test_handle_->throttle_runner_.CallResumeForTesting(); }

  void CancelDeferredNavigation(
      NavigationThrottle::ThrottleCheckResult result) {
    test_handle_->CancelDeferredNavigationInternal(result);
  }

  // Helper function to call WillStartRequest on |handle|. If this function
  // returns DEFER, |callback_result_| will be set to the actual result of
  // the throttle checks when they are finished.
  void SimulateWillStartRequest() {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;

    // It's safe to use base::Unretained since the NavigationHandle is owned by
    // the NavigationHandleImplTest.
    test_handle_->WillStartRequest(
        base::Bind(&NavigationHandleImplTest::UpdateThrottleCheckResult,
                   base::Unretained(this)));
  }

  // Helper function to call WillRedirectRequest on |handle|. If this function
  // returns DEFER, |callback_result_| will be set to the actual result of the
  // throttle checks when they are finished.
  // TODO(clamy): this should also simulate that WillStartRequest was called if
  // it has not been called before.
  void SimulateWillRedirectRequest() {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;

    // It's safe to use base::Unretained since the NavigationHandle is owned by
    // the NavigationHandleImplTest.
    test_handle_->WillRedirectRequest(
        GURL(), "GET", GURL(), false, scoped_refptr<net::HttpResponseHeaders>(),
        net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1, nullptr,
        base::Bind(&NavigationHandleImplTest::UpdateThrottleCheckResult,
                   base::Unretained(this)));
  }

  // Helper function to call WillFailRequest on |handle|. If this function
  // returns DEFER, |callback_result_| will be set to the actual result of the
  // throttle checks when they are finished.
  void SimulateWillFailRequest(
      net::Error net_error_code,
      const base::Optional<net::SSLInfo> ssl_info = base::nullopt) {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;
    test_handle_->set_net_error_code(net_error_code);

    // It's safe to use base::Unretained since the NavigationHandle is owned by
    // the NavigationHandleImplTest.
    test_handle_->WillFailRequest(
        main_test_rfh(), ssl_info,
        base::Bind(&NavigationHandleImplTest::UpdateThrottleCheckResult,
                   base::Unretained(this)));
  }

  // Helper function to call WillProcessResponse on |handle|. If this function
  // returns DEFER, |callback_result_| will be set to the actual result of the
  // throttle checks when they are finished.
  // TODO(clamy): this should also simulate that WillStartRequest was called if
  // it has not been called before.
  void SimulateWillProcessResponse() {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;

    // It's safe to use base::Unretained since the NavigationHandle is owned
    // by the NavigationHandleImplTest. The ConnectionInfo is different from
    // that sent to WillRedirectRequest to verify that it's correctly plumbed
    // in both cases.
    test_handle_->WillProcessResponse(
        main_test_rfh(), scoped_refptr<net::HttpResponseHeaders>(),
        net::HttpResponseInfo::CONNECTION_INFO_QUIC_35, net::HostPortPair(),
        net::SSLInfo(), GlobalRequestID(), false, false, false, false, false,
        base::Bind(&NavigationHandleImplTest::UpdateThrottleCheckResult,
                   base::Unretained(this)));
  }

  // Returns the handle used in tests.
  NavigationHandleImpl* test_handle() const { return test_handle_.get(); }

  // Whether the callback was called.
  bool was_callback_called() const { return was_callback_called_; }

  // Returns the callback_result.
  NavigationThrottle::ThrottleCheckResult callback_result() const {
    return callback_result_;
  }

  NavigationHandleImpl::State state() { return test_handle_->state(); }

  bool is_deferring() {
    switch (state()) {
      case NavigationHandleImpl::PROCESSING_WILL_START_REQUEST:
      case NavigationHandleImpl::PROCESSING_WILL_REDIRECT_REQUEST:
      case NavigationHandleImpl::PROCESSING_WILL_FAIL_REQUEST:
      case NavigationHandleImpl::PROCESSING_WILL_PROCESS_RESPONSE:
        return true;
      default:
        return false;
    }
  }

  bool call_counts_match(TestNavigationThrottle* throttle,
                         int start,
                         int redirect,
                         int failure,
                         int process) {
    return start == throttle->GetCallCount(
                        TestNavigationThrottle::WILL_START_REQUEST) &&
           redirect == throttle->GetCallCount(
                           TestNavigationThrottle::WILL_REDIRECT_REQUEST) &&
           failure == throttle->GetCallCount(
                          TestNavigationThrottle::WILL_FAIL_REQUEST) &&
           process == throttle->GetCallCount(
                          TestNavigationThrottle::WILL_PROCESS_RESPONSE);
  }

  // Creates, register and returns a TestNavigationThrottle that will
  // synchronously return |result| on checks by default.
  TestNavigationThrottle* CreateTestNavigationThrottle(
      NavigationThrottle::ThrottleCheckResult result) {
    TestNavigationThrottle* test_throttle =
        new TestNavigationThrottle(test_handle());
    test_throttle->SetResponseForAllMethods(TestNavigationThrottle::SYNCHRONOUS,
                                            result);
    test_handle()->RegisterThrottleForTesting(
        std::unique_ptr<TestNavigationThrottle>(test_throttle));
    return test_throttle;
  }

  // Creates, register and returns a TestNavigationThrottle that will
  // synchronously return |result| on check for the given |method|, and
  // NavigationThrottle::PROCEED otherwise.
  TestNavigationThrottle* CreateTestNavigationThrottle(
      TestNavigationThrottle::ThrottleMethod method,
      NavigationThrottle::ThrottleCheckResult result) {
    TestNavigationThrottle* test_throttle =
        CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
    test_throttle->SetResponse(method, TestNavigationThrottle::SYNCHRONOUS,
                               result);
    return test_throttle;
  }

  // Creates and register a NavigationThrottle that will delete the
  // NavigationHandle in checks.
  void AddDeletingNavigationThrottle() {
    DCHECK(test_handle_);
    test_handle()->RegisterThrottleForTesting(
        std::make_unique<DeletingNavigationThrottle>(
            test_handle(), base::BindRepeating(
                               &NavigationHandleImplTest::ResetNavigationHandle,
                               base::Unretained(this))));
  }

  void CreateNavigationHandle() {
    scoped_refptr<FrameNavigationEntry> frame_entry(new FrameNavigationEntry());
    request_ = NavigationRequest::CreateBrowserInitiated(
        main_test_rfh()->frame_tree_node(), CommonNavigationParams(),
        CommitNavigationParams(), false /* browser-initiated */, std::string(),
        *frame_entry, nullptr, nullptr, nullptr);
    test_handle_ =
        base::WrapUnique<NavigationHandleImpl>(new NavigationHandleImpl(
            request_.get(), GURL(), base::nullopt, std::vector<GURL>(),
            true,   // is_renderer_initiated
            false,  // is_same_document
            base::TimeTicks::Now(), 0,
            false,                  // started_from_context_menu
            CSPDisposition::CHECK,  // should_check_main_world_csp
            false,                  // is_form_submission
            nullptr,                // navigation_ui_data
            "GET", net::HttpRequestHeaders(),
            nullptr,  // resource_request_body
            Referrer(),
            false,  // has_user_gesture
            ui::PAGE_TRANSITION_LINK,
            false,  // is_external_protocol
            blink::mojom::RequestContextType::LOCATION,
            blink::WebMixedContentContextType::kBlockable,
            std::string(),        // href_translate
            base::TimeTicks()));  // input_start
  }

 private:
  // The callback provided to NavigationHandleImpl::WillStartRequest,
  // NavigationHandleImpl::WillRedirectRequest, and
  // NavigationHandleImpl::WillFailRequest during the tests.
  void UpdateThrottleCheckResult(
      NavigationThrottle::ThrottleCheckResult result) {
    callback_result_ = result;
    was_callback_called_ = true;
  }

  void ResetNavigationHandle() { test_handle_ = nullptr; }

  std::unique_ptr<NavigationRequest> request_;
  std::unique_ptr<NavigationHandleImpl> test_handle_;
  bool was_callback_called_;
  NavigationThrottle::ThrottleCheckResult callback_result_;
};

// Checks that the request_context_type is properly set.
// Note: can be extended to cover more internal members.
TEST_F(NavigationHandleImplTest, SimpleDataChecksRedirectAndProcess) {
  SimulateWillStartRequest();
  EXPECT_EQ(blink::mojom::RequestContextType::LOCATION,
            test_handle()->request_context_type());
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
            test_handle()->GetConnectionInfo());

  SimulateWillRedirectRequest();
  EXPECT_EQ(blink::mojom::RequestContextType::LOCATION,
            test_handle()->request_context_type());
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1,
            test_handle()->GetConnectionInfo());

  SimulateWillProcessResponse();
  EXPECT_EQ(blink::mojom::RequestContextType::LOCATION,
            test_handle()->request_context_type());
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_QUIC_35,
            test_handle()->GetConnectionInfo());
}

TEST_F(NavigationHandleImplTest, SimpleDataCheckNoRedirect) {
  SimulateWillStartRequest();
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
            test_handle()->GetConnectionInfo());

  SimulateWillProcessResponse();
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_QUIC_35,
            test_handle()->GetConnectionInfo());
}

TEST_F(NavigationHandleImplTest, SimpleDataChecksFailure) {
  SimulateWillStartRequest();
  EXPECT_EQ(blink::mojom::RequestContextType::LOCATION,
            test_handle()->request_context_type());
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
            test_handle()->GetConnectionInfo());

  SimulateWillFailRequest(net::ERR_CERT_DATE_INVALID);
  EXPECT_EQ(blink::mojom::RequestContextType::LOCATION,
            test_handle()->request_context_type());
  EXPECT_EQ(net::ERR_CERT_DATE_INVALID, test_handle()->GetNetErrorCode());
}

// Checks that a navigation deferred during WillStartRequest can be properly
// cancelled.
TEST_F(NavigationHandleImplTest, CancelDeferredWillStart) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_EQ(NavigationHandleImpl::INITIAL, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0));

  // Simulate WillStartRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillStartRequest();
  EXPECT_EQ(NavigationHandleImpl::PROCESSING_WILL_START_REQUEST, state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0));

  // Cancel the request. The callback should have been called.
  CancelDeferredNavigation(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_EQ(NavigationHandleImpl::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0));
}

// Checks that a navigation deferred during WillRedirectRequest can be properly
// cancelled.
TEST_F(NavigationHandleImplTest, CancelDeferredWillRedirect) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_EQ(NavigationHandleImpl::INITIAL, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0));

  // Simulate WillRedirectRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillRedirectRequest();
  EXPECT_EQ(NavigationHandleImpl::PROCESSING_WILL_REDIRECT_REQUEST, state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 1, 0, 0));

  // Cancel the request. The callback should have been called.
  CancelDeferredNavigation(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_EQ(NavigationHandleImpl::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 1, 0, 0));
}

// Checks that a navigation deferred during WillFailRequest can be properly
// cancelled.
TEST_F(NavigationHandleImplTest, CancelDeferredWillFail) {
  TestNavigationThrottle* test_throttle = CreateTestNavigationThrottle(
      TestNavigationThrottle::WILL_FAIL_REQUEST, NavigationThrottle::DEFER);
  EXPECT_EQ(NavigationHandleImpl::INITIAL, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0));

  // Simulate WillStartRequest.
  SimulateWillStartRequest();
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0));

  // Simulate WillFailRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillFailRequest(net::ERR_CERT_DATE_INVALID);
  EXPECT_EQ(NavigationHandleImpl::PROCESSING_WILL_FAIL_REQUEST, state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 1, 0));

  // Cancel the request. The callback should have been called.
  CancelDeferredNavigation(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_EQ(NavigationHandleImpl::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 1, 0));
}

// Checks that a navigation deferred can be canceled and not ignored.
TEST_F(NavigationHandleImplTest, CancelDeferredWillRedirectNoIgnore) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_EQ(NavigationHandleImpl::INITIAL, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0));

  // Simulate WillStartRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillStartRequest();
  EXPECT_EQ(NavigationHandleImpl::PROCESSING_WILL_START_REQUEST, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0));

  // Cancel the request. The callback should have been called with CANCEL, and
  // not CANCEL_AND_IGNORE.
  CancelDeferredNavigation(NavigationThrottle::CANCEL);
  EXPECT_EQ(NavigationHandleImpl::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0));
}

// Checks that a navigation deferred by WillFailRequest can be canceled and not
// ignored.
TEST_F(NavigationHandleImplTest, CancelDeferredWillFailNoIgnore) {
  TestNavigationThrottle* test_throttle = CreateTestNavigationThrottle(
      TestNavigationThrottle::WILL_FAIL_REQUEST, NavigationThrottle::DEFER);
  EXPECT_EQ(NavigationHandleImpl::INITIAL, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0));

  // Simulate WillStartRequest.
  SimulateWillStartRequest();
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0));

  // Simulate WillFailRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillFailRequest(net::ERR_CERT_DATE_INVALID);
  EXPECT_EQ(NavigationHandleImpl::PROCESSING_WILL_FAIL_REQUEST, state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 1, 0));

  // Cancel the request. The callback should have been called with CANCEL, and
  // not CANCEL_AND_IGNORE.
  CancelDeferredNavigation(NavigationThrottle::CANCEL);
  EXPECT_EQ(NavigationHandleImpl::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 1, 0));
}

// Checks that data from the SSLInfo passed into SimulateWillStartRequest() is
// stored on the handle.
TEST_F(NavigationHandleImplTest, WillFailRequestSetsSSLInfo) {
  uint16_t cipher_suite = 0xc02f;  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
  int connection_status = 0;
  net::SSLConnectionStatusSetCipherSuite(cipher_suite, &connection_status);

  // Set some test values.
  net::SSLInfo ssl_info;
  ssl_info.cert_status = net::CERT_STATUS_AUTHORITY_INVALID;
  ssl_info.connection_status = connection_status;

  SimulateWillStartRequest();
  SimulateWillFailRequest(net::ERR_CERT_DATE_INVALID, ssl_info);

  EXPECT_EQ(net::CERT_STATUS_AUTHORITY_INVALID,
            test_handle()->GetSSLInfo().cert_status);
  EXPECT_EQ(connection_status, test_handle()->GetSSLInfo().connection_status);
}

// Helper throttle which checks that it can access NavigationHandle's
// RenderFrameHost in WillFailRequest() and then defers the failure.
class GetRenderFrameHostOnFailureNavigationThrottle
    : public NavigationThrottle {
 public:
  GetRenderFrameHostOnFailureNavigationThrottle(NavigationHandle* handle)
      : NavigationThrottle(handle) {}
  ~GetRenderFrameHostOnFailureNavigationThrottle() override {}

  NavigationThrottle::ThrottleCheckResult WillFailRequest() override {
    EXPECT_TRUE(navigation_handle()->GetRenderFrameHost());
    return NavigationThrottle::DEFER;
  }

  const char* GetNameForLogging() override {
    return "GetRenderFrameHostOnFailureNavigationThrottle";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GetRenderFrameHostOnFailureNavigationThrottle);
};

// Verify that the NavigationHandle::GetRenderFrameHost() can be retrieved by a
// throttle in WillFailRequest(), as well as after deferring the failure.  This
// is allowed, since at that point the final RenderFrameHost will have already
// been chosen. See https://crbug.com/817881.
TEST_F(NavigationHandleImplTest, WillFailRequestCanAccessRenderFrameHost) {
  test_handle()->RegisterThrottleForTesting(
      std::make_unique<GetRenderFrameHostOnFailureNavigationThrottle>(
          test_handle()));
  SimulateWillStartRequest();
  SimulateWillFailRequest(net::ERR_CERT_DATE_INVALID);
  EXPECT_EQ(NavigationHandleImpl::PROCESSING_WILL_FAIL_REQUEST, state());
  EXPECT_TRUE(test_handle()->GetRenderFrameHost());
  Resume();
  EXPECT_TRUE(test_handle()->GetRenderFrameHost());
}

}  // namespace content