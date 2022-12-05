import 'dart:async';

import 'package:flutter/material.dart';
import 'package:samsung_tv_sign_in/samsung_tv_sign_in.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  String _userId = 'Unknown';
  final _samsungTvSignInPlugin = SamsungTvSignIn();

  bool _isSignedIn = false;

  @override
  void initState() {
    super.initState();
    initPlatformState();
  }

  Future<void> initPlatformState() async {
    _userId = await _samsungTvSignInPlugin.getLoginInfo();
    _isSignedIn = await _samsungTvSignInPlugin.getLoginState();
    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Samsung TV Sign In'),
        ),
        body: Center(
          child: Column(
            children: [
              if (_isSignedIn)
                const Text('Signed off')
              else
                Text('Signed in as: $_userId'),
              const SizedBox(height: 20),
              if (_isSignedIn)
                ElevatedButton(
                  onPressed: () {
                    setState(() {
                      _isSignedIn = false;
                    });
                  },
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.redAccent,
                  ),
                  child: const Text('Sign out'),
                )
              else
                ElevatedButton(
                  onPressed: () async {
                    _isSignedIn = await _samsungTvSignInPlugin.getLoginState();
                    setState(() {});
                  },
                  child: const Text('Sign in'),
                ),
            ],
          ),
        ),
      ),
    );
  }
}
