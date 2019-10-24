// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them.

#ifndef CHROME_BROWSER_SAFE_BROWSING_UI_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_UI_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/base_ui_manager.h"
#include "components/security_interstitials/content/unsafe_resource.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace history {
class HistoryService;
}  // namespace history

namespace safe_browsing {

struct HitReport;

// Construction needs to happen on the main thread.
class SafeBrowsingUIManager : public BaseUIManager {
 public:
  // Observer class can be used to get notified when a SafeBrowsing hit
  // is found.
  class Observer {
   public:
    // Called when |resource| is classified as unsafe by SafeBrowsing, and is
    // not whitelisted.
    // The |resource| must not be accessed after OnSafeBrowsingHit returns.
    // This method will be called on the UI thread.
    virtual void OnSafeBrowsingHit(const UnsafeResource& resource) = 0;

   protected:
    Observer() {}
    virtual ~Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  explicit SafeBrowsingUIManager(
      const scoped_refptr<SafeBrowsingService>& service);

  // Called to stop or shutdown operations on the UI thread. This may be called
  // multiple times during the life of the UIManager. Should be called
  // on UI thread. If shutdown is true, the manager is disabled permanently.
  void Stop(bool shutdown);

  // Called on the IO thread by the ThreatDetails with the serialized
  // protocol buffer, so the service can send it over.
  void SendSerializedThreatDetails(const std::string& serialized) override;

  // Calls |BaseUIManager::OnBlockingPageDone()| and triggers
  // |OnSecurityInterstitialProceeded| event if |proceed| is true.
  void OnBlockingPageDone(const std::vector<UnsafeResource>& resources,
                          bool proceed,
                          content::WebContents* web_contents,
                          const GURL& main_frame_url) override;

  // Report hits to unsafe contents (malware, phishing, unsafe download URL)
  // to the server. Can only be called on UI thread.  The hit report will
  // only be sent if the user has enabled SBER and is not in incognito mode.
  void MaybeReportSafeBrowsingHit(const safe_browsing::HitReport& hit_report,
                                  content::WebContents* web_contents) override;

  // Creates the whitelist URL set for tests that create a blocking page
  // themselves and then simulate OnBlockingPageDone(). OnBlockingPageDone()
  // expects the whitelist to exist, but the tests don't necessarily call
  // DisplayBlockingPage(), which creates it.
  static void CreateWhitelistForTesting(content::WebContents* web_contents);

  // Add and remove observers. These methods must be invoked on the UI thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* remove);

  // Adds an UnsafeResource |resource| for |url| to unsafe_resources_,
  // this should be called by the UrlCheckerDelegate whenever a resource load
  // is blocked due to a SB hit.
  void AddUnsafeResource(GURL url,
                         security_interstitials::UnsafeResource resource);

  // Checks if an UnsafeResource |resource| exists for |url|, if so, it is
  // removed from the vector, assigned to |resource| and the function returns
  // true. Otherwise the function returns false and nothing gets assigned to
  // |resource|.
  bool PopUnsafeResourceForURL(
      GURL url,
      security_interstitials::UnsafeResource* resource);

  const std::string app_locale() const override;
  history::HistoryService* history_service(
      content::WebContents* web_contents) override;
  const GURL default_safe_page() const override;

 protected:
  ~SafeBrowsingUIManager() override;

  // Creates a hit report for the given resource and calls
  // MaybeReportSafeBrowsingHit. This also notifies all observers in
  // |observer_list_|.
  void CreateAndSendHitReport(const UnsafeResource& resource) override;

  // Calls SafeBrowsingBlockingPage::ShowBlockingPage().
  void ShowBlockingPageForResource(const UnsafeResource& resource) override;

  // Helper method to ensure hit reports are only sent when the user has
  // opted in to extended reporting and is not currently in incognito mode.
  static bool ShouldSendHitReport(const HitReport& hit_report,
                                  content::WebContents* web_contents);

 private:
  friend class SafeBrowsingUIManagerTest;
  friend class TestSafeBrowsingUIManager;

  static GURL GetMainFrameWhitelistUrlForResourceForTesting(
      const safe_browsing::SafeBrowsingUIManager::UnsafeResource& resource);

  // Safebrowsing service.
  scoped_refptr<SafeBrowsingService> sb_service_;

  // Stores unsafe resources so they can be fetched from a navigation throttle
  // in the committed interstitials flow. Implemented as a pair vector since
  // most of the time it will be empty or contain a single element.
  std::vector<std::pair<GURL, security_interstitials::UnsafeResource>>
      unsafe_resources_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUIManager);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_UI_MANAGER_H_