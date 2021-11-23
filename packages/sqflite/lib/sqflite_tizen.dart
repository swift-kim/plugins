// ignore_for_file: implementation_imports

import 'dart:ffi';
import 'dart:io';

import 'package:path/path.dart';
import 'package:path_provider/path_provider.dart';
import 'package:sqflite/sqflite.dart';
import 'package:sqflite_common/src/database.dart';
import 'package:sqflite_common/src/factory.dart';
import 'package:sqflite_common_ffi/sqflite_ffi.dart';
import 'package:sqlite3/open.dart';

/// The plugin class.
class SqflitePlugin {
  SqflitePlugin._();

  /// Registers an FFI database factory as the default database factory.
  static void register() {
    open.overrideForAll(() => DynamicLibrary.open('libsqlite3.so.0'));

    databaseFactory = _TizenDatabaseFactory();
  }
}

class _TizenDatabaseFactory implements SqfliteDatabaseFactory {
  _TizenDatabaseFactory();

  final SqfliteDatabaseFactory _ffiDatabaseFactory =
      createDatabaseFactoryFfi(noIsolate: true) as SqfliteDatabaseFactory;

  @override
  Future<bool> databaseExists(String path) =>
      _ffiDatabaseFactory.databaseExists(path);

  @override
  Future<void> deleteDatabase(String path) =>
      _ffiDatabaseFactory.deleteDatabase(path);

  @override
  Future<String> getDatabasesPath() async {
    final Directory dataDir = await getApplicationDocumentsDirectory();
    return join(dataDir.path, 'sqflite_common_ffi', 'databases');
  }

  @override
  Future<Database> openDatabase(String path, {OpenDatabaseOptions? options}) =>
      _ffiDatabaseFactory.openDatabase(path, options: options);

  @override
  Future<void> setDatabasesPath(String path) =>
      _ffiDatabaseFactory.setDatabasesPath(path);

  @override
  Future<void> closeDatabase(SqfliteDatabase database) =>
      _ffiDatabaseFactory.closeDatabase(database);

  @override
  Future<T> invokeMethod<T>(String method, [dynamic arguments]) =>
      _ffiDatabaseFactory.invokeMethod<T>(method, arguments);

  @override
  SqfliteDatabase newDatabase(
          SqfliteDatabaseOpenHelper openHelper, String path) =>
      _ffiDatabaseFactory.newDatabase(openHelper, path);

  @override
  void removeDatabaseOpenHelper(String path) =>
      _ffiDatabaseFactory.removeDatabaseOpenHelper(path);

  @override
  Future<T> wrapDatabaseException<T>(Future<T> Function() action) =>
      _ffiDatabaseFactory.wrapDatabaseException<T>(action);
}
