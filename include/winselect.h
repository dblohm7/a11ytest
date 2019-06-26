/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef __ASPK_WINSELECT_H
#define __ASPK_WINSELECT_H

namespace aspk {

// NOT Thread safe!
HWND SelectWindow();

}  // namespace aspk

#endif  // __ASPK_WINSELECT_H
