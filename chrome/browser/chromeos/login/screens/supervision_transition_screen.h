// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SUPERVISION_TRANSITION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SUPERVISION_TRANSITION_SCREEN_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class SupervisionTransitionScreenView;
class BaseScreenDelegate;

class SupervisionTransitionScreen : public BaseScreen {
 public:
  SupervisionTransitionScreen(BaseScreenDelegate* base_screen_delegate,
                              SupervisionTransitionScreenView* view);
  ~SupervisionTransitionScreen() override;

  // BaseScreen:
  void Show() override;
  void Hide() override;

  // Called when view is destroyed so there's no dead reference to it.
  void OnViewDestroyed(SupervisionTransitionScreenView* view_);

  // Called when transition has finished, exits the screen.
  void OnSupervisionTransitionFinished();

 private:
  SupervisionTransitionScreenView* view_;

  DISALLOW_COPY_AND_ASSIGN(SupervisionTransitionScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SUPERVISION_TRANSITION_SCREEN_H_