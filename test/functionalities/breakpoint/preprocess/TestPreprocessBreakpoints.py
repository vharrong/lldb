"""
Test that lldb breakpoints work with preprocessed files
"""

import os, time
import unittest2
import re
import lldb
from lldbtest import *
import lldbutil

class PreprocessBreakpointTestCase(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    @dsym_test
    def test_preprocess_breakpoint_with_dsym(self):
        """Test that lldb breakpoint works for preprocessed file."""
        self.buildDsym(dictionary=self.d)
        # normally this is set in setUp(), but main2.cpp doesn't exist until buildDsym()
        self.d = {'CXX_SOURCES': self.source, 'EXE': self.exe_name}
        self.setTearDownCleanup(dictionary=self.d)
        self.hello_breakpoint()

    #@skipIfGcc # causes intermittent gcc debian buildbot failures, skip until we can investigate
    @dwarf_test
    def test_preprocess_breakpoint_with_dwarf(self):
        """Test that lldb breakpoint works for preprocessed file."""
        self.buildDwarf(dictionary=self.d)
        # normally this is set in setUp(), but main2.cpp doesn't exist until buildDwarf()
        self.d = {'CXX_SOURCES': self.source, 'EXE': self.exe_name}
        self.setTearDownCleanup(dictionary=self.d)
        self.hello_breakpoint()

    def setUp(self):
        # Call super's setUp().
        TestBase.setUp(self)
        self.source = 'main2.cpp'
        self.exe_name = self.testMethodName
        # dummy self.d, used to quiet assert that checks whether self.d has been set
        # self.d is set for real after buildXXXX()
        self.d = {'EXE': self.exe_name}

    def hello_breakpoint(self):
        #import pudb; pudb.set_trace()
        """Test that lldb breakpoint works for preprocessed file."""

        # Find the line number to break inside main().
        self.first_stop = line_number("main1.cpp", 'return 0;')

        exe = os.path.join(os.getcwd(), self.exe_name)
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        # Add a breakpoint.
        lldbutil.run_break_set_by_file_and_line (self, None, self.first_stop, num_expected_locations=1)

if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()
