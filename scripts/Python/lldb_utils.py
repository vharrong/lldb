"""Utility methods for building and maintaining lldb.

FindParentInParentChain -- Find a directory in the parent chain.
FindLLVMParentInParentChain -- Find 'llvm' in the parent chain.
FindInExecutablePath -- Find a program in the executable path.
PrintRemoveTreeCommandForPath -- print a command to remove a path.
RunInDirectory -- call given command in given directory.
FullPlatformName -- Return full platform, e.g., linux-x86_64.

"""


import calendar
import os
import platform
import re
import subprocess
import sys
import time
import workingdir


def FindParentInParentChain(item):
  """Find the closet path with the given name in the parent directory hierarchy.

  Args:
    item: relative path to be found (may contain a directory path (a/b/c)).

  Returns:
    The full path of found directory, or None if no such path was found.

  Raises:
    ValueError:  if given an absolute path.

  """
  if os.path.isabs(item):
    raise ValueError("FindParentInParentChain takes relative path")
  trydir = os.getcwd()
  while True:
    trypath = os.path.join(trydir, item)
    if os.path.exists(trypath):
      return trydir

    # loop to the parent directory, stopping once we've evaluated at the root.
    lastdir = trydir
    trydir = os.path.dirname(lastdir)
    if os.path.samefile(lastdir, trydir):
      return None


def _FindGitOrSvnControlledDirInParentChain(dir_name):
  """Find VC-controlled dir_name within parent dir chain.

  Args:

    dir_name: the directory name to find in the current directory or
      one of the parent directories up through the root of the current
      directory's file system. The directory parent chain is first
      checked for a dir_name child that is a git-controlled directory.
      If that fails to find dir_name within the parent chain, it
      checks to see if a subversion-controlled directory is present.

  Returns:
    The parent directory of the git/svn-controlled directory specified
    in the parent chain, or None when the directory specified is not
    found.
  """

  # first try assuming git repos
  parent = FindParentInParentChain(os.path.join(dir_name, ".git"))
  if parent:
    return parent

  # next try assuming svn
  return FindParentInParentChain(os.path.join(dir_name, ".svn"))


def FindLLVMParentInParentChain():
  """Find the llvm tree above us or at the same level."""
  return _FindGitOrSvnControlledDirInParentChain("llvm")


def FindInExecutablePath(prog):
  """Find the given program in the executable path.

  Args:
    prog: The program to find.

  Returns:
    The full pathname or None if prog not in path.

  """
  user_path = os.environ["PATH"]       # TODO(spucci) fix? on Windows...
  for pathdir in user_path.split(os.pathsep):
    pathdir = pathdir.rstrip("/")
    pathdir = pathdir.rstrip("\\")  # Windows
    try_path = os.path.join(pathdir, prog)
    if os.path.exists(try_path):
      return try_path
  return None


def PrintRemoveTreeCommandForPath(path):
  """Print the command to remove the given path.

  Useful for cut-and-paste from error message.

  Args:
    path: The root of the tree to remove.

  """
  print "You can remove this path with:"
  print "rm -rf " + path    # TODO(spucci): Fix Windows


def RunInDirectory(in_dir, command_tokens):
  """Run given command in a given directory.

  Will leave the cwd untouched on exit.

  Args:
    in_dir: directory in which to run the command
    command_tokens: tokens which comprise the command.

  Returns:
    The command status.

  Raises:
    TypeError: if there are missing arguments

  """

  if not command_tokens:
    raise TypeError("RunInDirectory requires directory and command tokens")

  # Go to directory, saving old path
  with workingdir.WorkingDir(in_dir, echo_changes=True):
    print " ".join(command_tokens)
    status = subprocess.call(command_tokens)
    if status != 0:
      print "command failed (see above)."

  return status


def FullPlatformName():
  """Return the full platform name, e.g., linux-x86_64."""

  if sys.platform.startswith("linux"):
    return "linux-" + platform.processor()
  else:
    raise TypeError("Unsupported architecture: " + sys.platform)
