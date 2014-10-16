#!/usr/bin/env python

"""Run configure for the lldb project.

 See lldb_configure.py -h for usage.

 Configures lldb to be build with either configure/(g)make (the
 default) or cmake/ninja (with an appropriate flag).

 This script checks to make sure that the system does
 not have a clang defined in the path.  Currently this will
 break our build on Goobuntu 12.04, where we assume we
 build with gcc 4.8+.

"""


# Python built-in modules
import argparse
import os
import subprocess


# Our modules
import lldb_utils
import workingdir


def ParseCommandLine():
  """Parse the command line and return a parser results object.

  Parse the command line arguments, returning a parser object
  suitable to control command execution decisions.

  Returns:
    A parser results object containing the results of parsing the command line.
  """
  parser = argparse.ArgumentParser(
      description="Configure lldb build environment.")

  parser.add_argument(
      "-target", action="store", dest="target", default="x86",
      help="specify the target type, x86, android (default: x86)")
  parser.add_argument(
      "-arch", action="store", dest="arch",
      help="specify the target arch type, x86, x86-android, armeabi")
  parser.add_argument(
      "-toolchain", action="store", dest="toolchain",
      help="specify the standalone toolchain dir (fullpath) for Android build")
  parser.add_argument(
      "-tblgen_dir", action="store", dest="tblgen_dir",
      help="specify the path to host machine's llvm-tblgen and clang-tblgen")
  parser.add_argument(
      "-a", "--enable-assertions", action="store_true", dest="enable_assertions",
      help="enable assert() runtime checks (default: disabled)")
  parser.add_argument(
      "-o", "--enable-optimized", action="store_true", dest="enable_optimized",
      help="generate optimized code (default: disabled)")
  parser.add_argument(
      "-s", "--debug-symbols", action="store_true", dest="enable_symbols",
      help="generate debug symbols (default: disabled)")
  parser.add_argument(
      "-c", "--cmake", action="store_true", dest="use_cmake",
      help="set configure system to cmake (default: configure)")
  parser.add_argument(
      "-n", "--ninja", action="store_true", dest="use_ninja",
      help="set build tool to ninja (default: make)")
  parser.add_argument(
      "-l", "--clang", action="store_true", dest="use_clang",
      help="set compiler to clang (default: gcc)")
  parser.add_argument(
      "-g", "--gold", action="store_true", dest="use_gold",
      help="set linker to gold (default: gnu ld)")
  parser.add_argument(
      "--ccache", action="store_true", dest="use_ccache",
      help="enable cached compiling (default: do not cache obj files)")
  parser.add_argument(
      "-u", "--unity", action="store_true", dest="use_unity",
      help="enable unity build compiles (default: one src file per obj file)")
  parser.add_argument(
      "-k", "--incremental-link", action="store_true", dest="use_inc_link",
      help="enable incremental linking with gold linker (default: no incremental linking)")
  parser.add_argument(
      "-b", "--build-dir", action="store",  dest="build_dir", default="build",
      help="specify the build dir, default: build")
  parser.add_argument(
      "-d", "--distcc", action="store_true", dest="use_distcc",
      help="enable distributed compiles to a build farm (default: compile locally)")
  parser.add_argument(
      "--coverage", action="store_true", dest="coverage",
      help="enable code coverage capture during exe runs. Default: no capture")
  parser.add_argument(
      "-i", "--install-dir", action="store",  dest="install_dir", default="install",
      help="specify the install dir, default: install")
  parser.add_argument(
      "--with-python-dir",
      action="store",
      
      default=os.environ.get("LLDB_PYTHON_BASE_DIR"),
      help="specify alternate python root dir")
  parser.add_argument(
      "-x", "--IDE-xcode", action="store_true", dest="ide_xcode",
      help="create project files for Xcode IDE (default: do not create Xcode IDE project)")
  parser.add_argument(
      "-v", "--IDE-visual-studio", action="store_true", dest="ide_visual_studio",
      help="create project files for MS Visual Studio IDE (default: do not create VS IDE project)")

  return parser.parse_args()


def AddFileForFindInProject(llvm_parent_dir):
  filename = os.path.join(llvm_parent_dir, "llvm", "tools", "lldb",
                          ".emacs-project")
  if os.path.exists(filename):
    os.remove(filename)
  f = open(filename, "w")
  f.write(r'(setl ffip-regexp ".*\\.\\(py\\|cpp\\|c\\|h\\)$")')
  f.write("\n")
  f.close()


def GetToolNames(args):
  """Construct names of the shell tools (config, make, cc, cpp, and linker) required for the given options.

  Args:
    args: the results from parsing the command line.

  Returns:
    The shell tool names required for the given options.

  """
  
  class Tool_Names: pass

  tool_names = Tool_Names()
  if args.target == "android":  
    if not args.toolchain:
        print("Missing standalone toolchain path, see -help")
        exit()
        
    if not args.tblgen_dir:
    	print("Missing llvm-tblgen and clang-tblgen path, see -help")
    	exit()
    	
    if not args.arch:
        print("Missing arch specification, see -help")
        exit()           
        
    args.use_cmake = 1;
    tool_names.config = ("cmake")
    args.use_ninja = 1;
    tool_names.make = ("ninja")
    tool_names.cc = ("gcc")
    tool_names.cxx = ("g++")
    tool_names.ld = ("ld")
  else:
    tool_names.config = ("configure" if not args.use_cmake else "cmake")
    tool_names.make = ("make" if not args.use_ninja else "ninja")
    tool_names.cc = ("gcc" if not args.use_clang else "clang")
    tool_names.cxx = ("g++" if not args.use_clang else "clang++")
    tool_names.ld = ("ld" if not args.use_gold else "ld.gold")

  # add distcc prefix if enabled
  if args.use_distcc:
    tool_names.cc = "distcc " + tool_names.cc
    tool_names.cxx = "distcc " + tool_names.cxx

  # ccache should be the initial compiler prefix (before distcc),
  # so all subsequent compile operations are skipped on a cache hit
  if args.use_ccache:
    tool_names.cc = "ccache " + tool_names.cc
    tool_names.cxx = "ccache " + tool_names.cxx

    # if using ccache with clang, add extra clang args to silence 'unused -I' warnings.
    # args added to tool name instead of compiler flags because flags are ignored/overridden
    # by some of lldb's cmake build script
    if args.use_clang:
      tool_names.cc = tool_names.cc + " -Qunused-arguments -fcolor-diagnostics"
      tool_names.cxx = tool_names.cxx + " -Qunused-arguments -fcolor-diagnostics"

  return tool_names

def GetCxxFlags(args):
  """Construct C++ compiler flags required for the given options.

  Args:
    args: the results from parsing the command line.

  Returns:
    The C++ compiler flags required for the given options.

  """
  flags = ""
  
  if args.coverage:
    # add code coverage flags
    flags += " -fprofile-arcs -ftest-coverage"

  if args.with_python_dir:
    flags += " -I%s" % os.path.join(
        args.with_python_dir, "include", "python2.7")

  return flags


def GetLdFlags(args):
  """Construct linker flags required for the given options.

  Args:
    args: the results from parsing the command line.

  Returns:
    The C++ compiler flags required for the given options.

  """
  flags = ""

  if args.coverage:
    # add code coverage flags
    flags += " -fprofile-arcs -ftest-coverage"

  if args.with_python_dir:
    flags += " -L{} -L{}".format(
        os.path.join(args.with_python_dir, "lib"),
        os.path.join(args.with_python_dir, "lib", "python2.7", "config"))

  return flags


def main():
  args = ParseCommandLine()

  # find the parent of the llvm directory
  llvm_parent_dir = lldb_utils.FindLLVMParentInParentChain()
  print "Found llvm parent dir: " + (llvm_parent_dir
                                     if llvm_parent_dir else "<none>")

  # Fail if no such place
  if not llvm_parent_dir:
    print "Error: No llvm directory found in parent chain."
    exit(1)

  AddFileForFindInProject(llvm_parent_dir)
  build_dir = os.path.join(llvm_parent_dir, args.build_dir)
  install_dir = os.path.join(llvm_parent_dir, args.install_dir)

  # fail if the build directory already exists
  if os.path.exists(build_dir):
    print "Error: build directory must not already exist: " + build_dir
    print "Please delete before re-running."
    lldb_utils.PrintRemoveTreeCommandForPath(build_dir)
    exit(1)

  # fail if the install directory already exists
  if os.path.exists(install_dir):
    print "Error: install directory must not already exist: " + install_dir
    print "Please delete before re-running."
    lldb_utils.PrintRemoveTreeCommandForPath(install_dir)
    exit(1)

  scripts_dir = os.path.dirname(os.path.realpath(__file__))
  lldb_dir = os.path.dirname(scripts_dir)

  # if code coverage is enabled, we must have debug info, either by
  # --release-debug or debug (i.e. release must not be set)
  if args.coverage and not args.enable_symbols:
    print("Error: code coverage requires debug info in the build.\n"
          "Add --debug-symbols to the command line.")
    exit(1)

  # get names of config, make, compilers, & linker
  tool_names = GetToolNames(args)

  # get flag arguments for the compiler and linker
  cxx_flags = GetCxxFlags(args)
  ld_flags = GetLdFlags(args)

  # Make build directory
  os.makedirs(build_dir)

  with workingdir.WorkingDir(build_dir):

    build_type_name = ""
    if args.enable_symbols and args.enable_assertions:
      build_type_name = "Debug"
    if args.enable_symbols and args.enable_optimized:
      build_type_name = "RelWithDebInfo"
    elif args.enable_optimized:
      build_type_name = "Release"
      
    config_message = "configured for " + tool_names.config + "/" + tool_names.make + " (%s)" % build_type_name

    if args.enable_symbols:
      # we need to add -g to tell it to include debug info in the
      # release build
      cxx_flags += " -g"

    if args.use_cmake:
      if args.target == "android":
        print("Configuring for " + args.target + ", " + args.arch + ", " + args.toolchain)
    	# convert to cmake script style string
    	# todo: 32bit x86
    	if args.arch == "x86-android":
        	args.arch = "x86_64"
        	llvm_target_arch = "X86"
        	llvm_targets_to_build = "X86"
        # todo: 64bit arm and mips
        else:
        	llvm_target_arch = "ARM"
        	llvm_targets_to_build = "ARM"
        	
        llvm_tblgen = args.tblgen_dir + "/llvm-tblgen"
        clang_tblgen = args.tblgen_dir + "/clang-tblgen"
        if not os.path.isfile(llvm_tblgen):
        	print "Can't find " + llvm_tblgen
        	exit()
        if not os.path.isfile(clang_tblgen):
        	print "Can't find " + clang_tblgen
        	exit()
        command_tokens = ("cmake",
                         ("" if not args.use_ninja else "-GNinja"),
                          "-DCMAKE_TOOLCHAIN_FILE=../lldb-tools/android/android.toolchain.cmake",
                          "-DANDROID_STANDALONE_TOOLCHAIN=" + args.toolchain,
                          "-DPYTHON_EXECUTABLE=" + args.toolchain + "/bin/python",
                          "-DANDROID_TOOLCHAIN_NAME=standalone",                              
                          "-DCMAKE_CXX_COMPILER_VERSION=4.8",                          
                          "-DANDROID_ABI=" + args.arch,                          
                          "-DANDROID_STL=none",
                          "-DCMAKE_INSTALL_PREFIX:PATH=" + install_dir,
                          "-DCMAKE_BUILD_TYPE=" + build_type_name,
                          "-DLLVM_TARGET_ARCH=" + llvm_target_arch,
                          "-DLLVM_TARGETS_TO_BUILD=" + llvm_targets_to_build,
                          "-DLLVM_TABLEGEN=" + llvm_tblgen,
                          "-DCLANG_TABLEGEN=" + clang_tblgen, 
                          # Do not include this next flag if you want to see
                          # cmake maintainer-related messages.
                          "-Wno-dev",
                          os.path.join("..", "llvm"))
      else:
          command_tokens = ("cmake",
                           ("" if not args.use_ninja else "-GNinja"),
                          "-DCMAKE_LINKER=" + tool_names.ld,
                          "-DCMAKE_CXX_FLAGS=" + cxx_flags,
                          "-DCMAKE_SHARED_LINKER_FLAGS=" + ld_flags,
                          "-DCMAKE_EXE_LINKER_FLAGS=" + ld_flags,
                          "-DCMAKE_INSTALL_PREFIX:PATH=" + install_dir,
                          "-DCMAKE_BUILD_TYPE=" + build_type_name,
                          # Do not include this next flag if you want to see
                          # cmake maintainer-related messages.
                          "-Wno-dev",
                          os.path.join("..", "llvm"))            
    else:
      command_tokens = [os.path.join("..", "llvm", "configure"),
                        "--enable-cxx11",
                        "--prefix=%s" % install_dir]

      if args.enable_optimized:
        command_tokens.append("--enable-optimized")
      else:
        command_tokens.append("--disable-optimized")

      if args.enable_assertions:
        command_tokens.append("--enable-assertions")
      else:
        command_tokens.append("--disable-assertions")

      command_tokens.append("--with-extra-options=%s" % cxx_flags)
      command_tokens.append("--with-extra-ld-options=%s" % ld_flags)

    env_vars = os.environ
    # these environment settings really mess up the Android cmake file
    if args.target != 'android':
      env_vars["CC"] = tool_names.cc
      env_vars["CXX"] = tool_names.cxx
      env_vars["DISTCC_HOSTS"] = ""
      
      if args.use_ccache and args.use_clang:
        env_vars["CCACHE_CPP2"] = "yes"  # This is intended to silence preproc -I warnings, but is not working as expected

      print "CC='" + tool_names.cc + "' CXX='" + tool_names.cxx + "' " + " ".join(command_tokens)

    status = subprocess.call(command_tokens, env=env_vars)
    #    status = 0;
    if status != 0:
      print "configure command failed (see above)."
      exit(1)

    print ""
    print config_message

    if args.with_python_dir:
      print "using custom python ({})".format(args.with_python_dir)
    else:
      print "using stock system python"

    print "The build directory has been set up:"
    print "cd " + build_dir


if __name__ == "__main__":
  main()
