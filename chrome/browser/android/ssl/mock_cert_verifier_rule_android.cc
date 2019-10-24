// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ssl/mock_cert_verifier_rule_android.h"

#include "base/command_line.h"
#include "jni/MockCertVerifierRuleAndroid_jni.h"

jlong JNI_MockCertVerifierRuleAndroid_Init(JNIEnv* env, jint result) {
  return reinterpret_cast<intptr_t>(new MockCertVerifierRuleAndroid(result));
}

MockCertVerifierRuleAndroid::MockCertVerifierRuleAndroid(int result) {
  mock_cert_verifier_.mock_cert_verifier()->set_default_result(result);
}

void MockCertVerifierRuleAndroid::SetUp(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  mock_cert_verifier_.SetUpCommandLine(base::CommandLine::ForCurrentProcess());
  mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void MockCertVerifierRuleAndroid::TearDown(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
}