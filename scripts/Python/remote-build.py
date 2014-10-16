#!/usr/bin/python

from __future__ import print_function

import argparse
import getpass
import os
import os.path
import re
import select
import sys
import subprocess

_COMMON_SYNC_OPTS = "-avzhe ssh --delete"
_COMMON_EXCLUDE_OPTS = "--exclude=DerivedData --exclude=.svn --exclude=.git --exclude=llvm-build/Release+Asserts"

def normalize_configuration(config_text):
    if not config_text:
        return "debug"

    config_lower = config_text.lower()
    if config_lower in ["debug", "release"]:
        return config_lower
    else:
        raise Exception("unknown configuration specified: %s" % config_text)

def parse_args():
    DEFAULT_REMOTE_ROOT_DIR = "/mnt/ssd/work/macosx.sync"
    DEFAULT_REMOTE_HOSTNAME = "tfiala2.mtv.corp.google.com"
    OPTIONS_FILENAME = '.remote-build.conf'

    parser = argparse.ArgumentParser(fromfile_prefix_chars='@')

    parser.add_argument(
        "--ccache",
        action="store_true",
        dest="use_ccache",
        help="use ccache in the remote build")
    parser.add_argument(
        "--configuration", "-c",
        help="specify configuration (Debug, Release)",
        default=normalize_configuration(os.environ.get('CONFIGURATION', 'Debug')))
    parser.add_argument(
        "--local-lldb-dir", "-l", metavar="DIR",
        help="specify local lldb directory (Xcode layout assumed for llvm/clang)",
        default=os.getcwd())
    parser.add_argument(
        "--remote-address", "-r", metavar="REMOTE-ADDR",
        help="specify the dns name or ip address of the remote linux system",
        default=DEFAULT_REMOTE_HOSTNAME)
    parser.add_argument(
        "--remote-dir", "-d", metavar="DIR",
        help="specify the root of the linux source/build dir",
        default=DEFAULT_REMOTE_ROOT_DIR)
    parser.add_argument(
        "--user", "-u", help="specify the user name for the remote system",
        default=getpass.getuser())
    parser.add_argument(
        "--xcode-action", "-x", help="$(ACTION) from Xcode", nargs='?', default=None)

    command_line_args = sys.argv[1:]
    if os.path.exists(OPTIONS_FILENAME):
        # Prepend the file so that command line args override the file contents.
        command_line_args.insert(0, "@%s" % OPTIONS_FILENAME)

    return parser.parse_args(command_line_args)


def maybe_create_remote_root_dir(args):
    commandline = [
        "ssh",
        "%s@%s" % (args.user, args.remote_address),
        "mkdir",
        "-p",
        args.remote_dir]
    return subprocess.call(commandline)


def init_with_args(args):
    # Expand any user directory specs in local-side source dir (on MacOSX).
    args.local_lldb_dir = os.path.expanduser(args.local_lldb_dir)

    # Append the configuration type to the remote build dir.
    args.configuration = normalize_configuration(args.configuration)
    args.remote_build_dir = os.path.join(
        args.remote_dir,
        "build-%s" % args.configuration)

    # We assume the local lldb directory is really named 'lldb'.
    # This is because on the remote end, the local lldb root dir
    # is copied over underneath llvm/tools and will be named there
    # whatever it is named locally.  The remote build will assume
    # is is called lldb.
    if os.path.basename(args.local_lldb_dir) != 'lldb':
        raise Exception(
            "local lldb root needs to be called 'lldb' but was {} instead"
            .format(os.path.basename(args.local_lldb_dir)))

    args.lldb_dir_relative_regex = re.compile("%s/llvm/tools/lldb/" % args.remote_dir)
    args.llvm_dir_relative_regex = re.compile("%s/" % args.remote_dir)

    print("Xcode action:", args.xcode_action)

    # Ensure the remote directory exists.
    result = maybe_create_remote_root_dir(args)
    if result == 0:
        print("using remote root dir: %s" % args.remote_dir)
    else:
        print("remote root dir doesn't exist and could not be created, "
              + "error code:", result)
        return False

    return True

def sync_llvm(args):
    commandline = ['rsync']
    commandline.extend(_COMMON_SYNC_OPTS.split())
    commandline.extend(_COMMON_EXCLUDE_OPTS.split())
    commandline.append("--exclude=/llvm/tools/lldb")
    commandline.extend([
        "%s/llvm" % args.local_lldb_dir,
        "%s@%s:%s" % (args.user, args.remote_address, args.remote_dir)])
    return subprocess.call(commandline)


def sync_lldb(args):
    commandline = ['rsync']
    commandline.extend(_COMMON_SYNC_OPTS.split())
    commandline.extend(_COMMON_EXCLUDE_OPTS.split())
    commandline.extend([
        "--exclude=/lldb/llvm",
        args.local_lldb_dir,
        "%s@%s:%s/llvm/tools" % (args.user, args.remote_address, args.remote_dir)])
    return subprocess.call(commandline)


def maybe_configure(args):
    commandline = [
        "ssh",
        "%s@%s" % (args.user, args.remote_address),
        "cd", args.remote_dir, "&&",
        "touch", "llvm/.git", "&&",
        os.path.join(".", "llvm","tools","lldb","scripts","Python","lldb_configure.py"),
        "-a", # enable assertions
        "-b", args.remote_build_dir, # use this build dir
        "-c", # use cmake
        "-g", # use gold linker
        "-l", # use clang
        "-n", # use ninja
        "-s", # generate debug symbols
        ]

    if args.use_ccache:
        commandline.append("--ccache")

    if args.configuration == 'release':
        commandline.append('--release')

    return subprocess.call(commandline)


def filter_build_line(args, line):
    lldb_relative_line = args.lldb_dir_relative_regex.sub('', line)
    if len(lldb_relative_line) != len(line):
        # We substituted - return the modified line
        return lldb_relative_line

    # No match on lldb path (longer on linux than llvm path).  Try
    # the llvm path match.
    return args.llvm_dir_relative_regex.sub('', line)


def run_remote_build_command(args, build_command_list):
    commandline = [
        "ssh", "%s@%s" % (args.user, args.remote_address),
        "cd", args.remote_build_dir, "&&"]
    commandline.extend(build_command_list)

    proc = subprocess.Popen(
        commandline,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)

    # Filter stdout/stderr output for file path mapping.
    # We do this to enable Xcode to see filenames relative to the
    # MacOSX-side directory structure.
    while True:
        reads = [proc.stdout.fileno(), proc.stderr.fileno()]
        select_result = select.select(reads, [], [])

        for fd in select_result[0]:
            if fd == proc.stdout.fileno():
                line = proc.stdout.readline()
                print(filter_build_line(args, line.rstrip()))
            elif fd == proc.stderr.fileno():
                line = proc.stderr.readline()
                print(filter_build_line(args, line.rstrip()), file=sys.stderr)

        proc_retval = proc.poll()
        if proc_retval != None:
            # Process stopped.  Drain output before finishing up.

            # Drain stdout.
            while True:
                line = proc.stdout.readline()
                if line:
                    print(filter_build_line(args, line.rstrip()))
                else:
                    break

            # Drain stderr.
            while True:
                line = proc.stderr.readline()
                if line:
                    print(filter_build_line(args, line.rstrip()), file=sys.stderr)
                else:
                    break

            return proc_retval


def build(args):
    return run_remote_build_command(args, ["time", "ninja"])


def clean(args):
    return run_remote_build_command(args, ["ninja", "clean"])


if __name__ == "__main__":
    # Handle arg parsing.
    args = parse_args()

    # Initialize the system.
    if not init_with_args(args):
        exit(1)

    # Sync over llvm and clang source.
    sync_llvm(args)

    # Sync over lldb source.
    sync_lldb(args)

    # Configure the remote build if it's not already.
    maybe_configure(args)

    if args.xcode_action == 'clean':
        exit(clean(args))
    else:
        exit(build(args))
