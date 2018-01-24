#!/usr/bin/env python
# Copyright 2017 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import errno
import json
import os
import shutil
import sys


def make_dir(path, is_dir=False):
    """Creates the directory at `path`."""
    target = path if is_dir else os.path.dirname(path)
    try:
        os.makedirs(target)
    except OSError as exception:
        if exception.errno == errno.EEXIST and os.path.isdir(target):
            pass
        else:
            raise


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out-dir',
                        help='Path to the directory where to install the SDK',
                        required=True)
    parser.add_argument('--stamp-file',
                        help='Path to the victory file',
                        required=True)
    parser.add_argument('--manifest',
                        help='Path to the SDK\'s manifest file',
                        required=True)
    parser.add_argument('--domains',
                        help='List of domains to export',
                        nargs='*')
    parser.add_argument('--depname',
                        help='Name of the depfile target',
                        required=True)
    parser.add_argument('--depfile',
                        help='Path to the depfile to generate',
                        required=True)
    args = parser.parse_args()

    if len(args.domains) != 1 and args.domains[0] != 'c-pp':
        print('Only the "c-pp" domain is supported at the moment.')
        return 1

    # Remove any existing output.
    shutil.rmtree(args.out_dir, True)

    with open(args.manifest, 'r') as manifest_file:
        manifest = json.load(manifest_file)
    atoms = filter(lambda a: a['id']['domain'] == 'c-pp', manifest['atoms'])

    base_dir = os.path.join(args.out_dir, 'pkg')
    for atom in atoms:
        dir = os.path.join(base_dir, atom['id']['name'])
        for relative_destination, source in atom['files'].iteritems():
            destination = os.path.join(dir, relative_destination)
            make_dir(destination)
            shutil.copyfile(source, destination)

    with open(args.depfile, 'w') as dep_file:
        dep_file.write('%s:\n' % args.depname)

    with open(args.stamp_file, 'w') as stamp_file:
        stamp_file.write('Now go use it\n')


if __name__ == '__main__':
    sys.exit(main())
