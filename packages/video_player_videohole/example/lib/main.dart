// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ignore_for_file: public_member_api_docs, avoid_print

/// An example of using the plugin, controlling lifecycle and playback of the
/// video.

import 'package:flutter/material.dart';

import 'video_player_videohole.dart' if (dart.library.html) 'video_player.dart';

void main() {
  runApp(
    MaterialApp(
      home: _App(),
    ),
  );
}

class _App extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return DefaultTabController(
      length: 1,
      child: Scaffold(
        key: const ValueKey<String>('home_page'),
        appBar: AppBar(
          title: const Text('Video player example'),
          bottom: const TabBar(
            isScrollable: true,
            tabs: <Widget>[
              Tab(icon: Icon(Icons.cloud), text: 'MP4'),
            ],
          ),
        ),
        body: TabBarView(
          children: <Widget>[
            Mp4RemoteVideo(),
          ],
        ),
      ),
    );
  }
}
