// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io';

import 'package:io/ansi.dart';
import 'package:fxtest/fxtest.dart';
import 'package:meta/meta.dart';

/// Container that holds enough information about a test to execute it and
/// report back on output / events.
class TestBundle {
  /// Wrapper around the individual strings needed to run this test.
  final TestDefinition testDefinition;

  // That which actually launches a process to run the test.
  final TestRunner testRunner;

  /// Sink for realtime updates from the running test process.
  final Function(String) _realtimeOutputSink;

  /// Copy of all output, used to send to user when a timeout happens and
  /// [_realtimeOutputSink] is null.
  final StringBuffer _outputBuffer;

  /// The directory from which our test assumes it was invoked.
  final String workingDirectory;

  /// Tokens to pass through to the test runners.
  final List<String> runnerFlags;

  /// Tokens to pass through to individual tests.
  final List<String> extraFlags;

  /// Flag to disable actually running the test.
  final bool isDryRun;

  /// Flag to raise an exception on any test failure which will bubble all the
  /// way up and halt suite execution. Useful when users want "fail-fast"
  /// behavior.
  final bool raiseOnFailure;

  TestBundle(
    this.testDefinition, {
    @required this.testRunner,
    @required this.workingDirectory,
    this.runnerFlags = const [],
    this.extraFlags = const [],
    this.raiseOnFailure = false,
    this.isDryRun = false,
    Function(String) realtimeOutputSink,
  })  : _realtimeOutputSink = realtimeOutputSink,
        _outputBuffer = StringBuffer() {
    if (testRunner == null) {
      throw AssertionError('`testRunner` must not equal `null`');
    }
  }

  factory TestBundle.build({
    @required TestDefinition testDefinition,
    @required TestsConfig testsConfig,
    @required String workingDirectory,
    @required TestRunner testRunner,
    Function(String) realtimeOutputSink,
  }) =>
      TestBundle(
        testDefinition,
        extraFlags: testsConfig.testArguments.passThroughArgs,
        isDryRun: testsConfig.flags.dryRun,
        raiseOnFailure: testsConfig.flags.shouldFailFast,
        runnerFlags: testsConfig.runnerTokens,
        realtimeOutputSink: realtimeOutputSink ?? (String val) => null,
        testRunner: testRunner,
        workingDirectory: workingDirectory,
      );

  Function(String) get realtimeOutputSink => (String val) {
        if (_realtimeOutputSink != null) {
          _realtimeOutputSink(val);
        } else {
          _outputBuffer.writeln(val);
        }
      };

  /// Invokes the actual test that this class wraps.
  ///
  /// Returns a stream of test events that send feedback to the user.
  Stream<TestEvent> run() async* {
    var testType = testDefinition.executionHandle.testType;
    if (testType == TestType.unsupportedDeviceTest) {
      var greyTestName =
          wrapWith(testDefinition.executionHandle.handle, [styleBold]);
      yield TestInfo(
        'Skipping unrunnable legacy test: "$greyTestName". '
        'All device tests must be component-tests, but this is a binary',
      );
      return;
    }

    CommandTokens commandTokens =
        testDefinition.executionHandle.getInvocationTokens(runnerFlags);

    // Unparsed tests imply a major problem with `fx test`, so we
    // definitely want to throw an exception
    if (commandTokens.tokens == null || commandTokens.tokens.isEmpty) {
      throw UnrunnableTestException(
        'Failed to determine run context for test:\n$testDefinition',
      );
    }

    // Defer this check to now (as opposed to when the test is compiled), so the
    // feedback can be synced to when tests are executed.
    if (commandTokens.warning != null && commandTokens.warning != '') {
      yield TestInfo(commandTokens.warning);
    }

    String fullCommand = commandTokens.fullCommand;
    yield TestStarted(testDefinition: testDefinition, testName: fullCommand);

    if (isDryRun) {
      yield TestResult.skipped(testName: fullCommand);
      return;
    }
    DateTime start = DateTime.now();
    ProcessResult result = await testRunner.run(
      commandTokens.command,
      commandTokens.args..addAll(extraFlags),
      workingDirectory: workingDirectory,
      realtimeErrorSink: realtimeOutputSink,
      realtimeOutputSink: realtimeOutputSink,
    );

    yield TestResult(
      testName: fullCommand,
      exitCode: result.exitCode,
      runtime: DateTime.now().difference(start),
      message: result.exitCode == 0
          ? result.stdout
          : _formatError(
              fullCommand,
              result,
            ),
    );
    if (raiseOnFailure && result.exitCode != 0) {
      throw FailFastException();
    }
  }

  String _formatError(String cmd, ProcessResult result) {
    List<String> resultStdout = result.stdout != ''
        ? [...result.stdout.split('\n'), '\n']
            .where((var val) => val != '')
            .cast<String>()
            .toList()
        : [];
    return [
      wrapWith('> $cmd', [red]),
      ...resultStdout,
      result.stderr,
    ].join('\n');
  }

  @override
  String toString() => [
        '<TestBundle',
        '  testName: ${testDefinition.name}',
        '  extraFlags: $extraFlags',
        '  runnerFlags: $runnerFlags',
        '/>',
      ].join('\n');
}
