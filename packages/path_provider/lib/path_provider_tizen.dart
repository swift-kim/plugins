// Copyright 2021 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export 'src/path_provider_tizen_stub.dart'
    if (dart.library.ffi) 'src/path_provider_tizen_real.dart';
