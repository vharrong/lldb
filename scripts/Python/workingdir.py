"""Provides a working directory context manager.

   Provides a working directory object for use with Python's
   "with" statement. This context object allows code to change directories
   for a code block excursion and cleanly restore the state of the working
   directory regardless of how the code block is exited.

Example:

   import workingdir
   import os.path

   with workingdir.WorkingDir(os.path.join("some","path")):
     # do operations in "some/path"
     # some of which might throw an exception

   # when you get back here, the directory is restored
   # to the directory before the with statement was executed,
   # irrespective of whether an exception or normal block execution
   # was the cause of leaving the block.
"""

import os


class WorkingDir(object):
  """Context manager class for changing the working directory with restore.
  """

  def __init__(self, newWorkingDir, echo_changes=False):
    # save current directory
    self._initial_dir = os.getcwd()
    self._target_dir = newWorkingDir
    self._echo_changes = echo_changes

  def __enter__(self):
    # change into the dir
    if self._initial_dir != self._target_dir:
      if self._echo_changes and self._target_dir != ".":
        print "cd " + self._target_dir
      os.chdir(self._target_dir)

  def __exit__(self, exc_type, exc_value, traceback):
    # revert to the starting directory regardless of any exceptions
    if self._initial_dir != self._target_dir:
      if self._echo_changes and self._target_dir != ".":
        print "cd " + self._initial_dir
      os.chdir(self._initial_dir)
