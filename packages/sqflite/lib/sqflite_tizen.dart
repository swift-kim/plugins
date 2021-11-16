import 'dart:ffi';
import 'dart:io';

import 'package:path_provider/path_provider.dart';
import 'package:sqflite/sqflite.dart';
import 'package:sqflite_common_ffi/sqflite_ffi.dart';
import 'package:sqlite3/open.dart';

/// The plugin class.
class SqflitePlugin {
  SqflitePlugin._();

  /// Sets the current working directory and the default database factory
  /// instance for the sqflite plugin.
  static void register() {
    getApplicationDocumentsDirectory().then((Directory dir) {
      Directory.current = dir.path;
    });

    open.overrideForAll(() => DynamicLibrary.open('libsqlite3.so.0'));

    databaseFactory = createDatabaseFactoryFfi(noIsolate: true);
  }
}
