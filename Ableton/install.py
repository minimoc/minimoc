"""
Installation helper for AutoTrackCreator.

Copies (or symlinks) the AutoTrackCreator package to Ableton's
system MIDI Remote Scripts directory.

Usage:
    python install.py [--symlink]
"""

import os
import sys
import shutil
import argparse


def find_ableton_paths():
    programdata = os.environ.get('PROGRAMDATA', r'C:\ProgramData')
    ableton_base = os.path.join(programdata, 'Ableton')

    system_path = None
    if os.path.isdir(ableton_base):
        for item in sorted(os.listdir(ableton_base), reverse=True):
            scripts_dir = os.path.join(
                ableton_base, item, 'Resources', 'MIDI Remote Scripts'
            )
            if os.path.isdir(scripts_dir):
                system_path = scripts_dir
                break

    appdata = os.environ.get('APPDATA', '')
    ableton_user = os.path.join(appdata, 'Ableton')
    user_path = None

    if os.path.isdir(ableton_user):
        versions = sorted(
            [d for d in os.listdir(ableton_user) if d.startswith('Live')],
            reverse=True,
        )
        for version in versions:
            candidate = os.path.join(
                ableton_user, version, 'Preferences', 'User Remote Scripts'
            )
            if os.path.isdir(candidate):
                user_path = candidate
                break

    return system_path, user_path


def install(source_dir, target_dir, use_symlink=False):
    dest = os.path.join(target_dir, 'AutoTrackCreator')

    if os.path.exists(dest):
        print('Removing existing installation at: {}'.format(dest))
        if os.path.islink(dest):
            os.unlink(dest)
        else:
            shutil.rmtree(dest)

    if use_symlink:
        os.symlink(source_dir, dest, target_is_directory=True)
        print('Created symlink: {} -> {}'.format(dest, source_dir))
    else:
        shutil.copytree(source_dir, dest)
        print('Copied to: {}'.format(dest))

    print()
    print('Installation complete!')
    print()
    print('Next steps:')
    print('  1. (Re)start Ableton Live')
    print('  2. Preferences > Link/Tempo/MIDI')
    print('  3. Select "AutoTrackCreator" as Control Surface')
    print('  4. A MIDI track will be created automatically')
    print('  5. Check Log.txt for available MIDI devices')
    print('  6. Edit config.json to set your preferred routing:')
    print('     {}'.format(os.path.join(dest, 'config.json')))


def main():
    parser = argparse.ArgumentParser(
        description='Install AutoTrackCreator to Ableton'
    )
    parser.add_argument(
        '--symlink', action='store_true',
        help='Create symlink instead of copy (requires admin on Windows)'
    )
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    source = os.path.join(script_dir, 'AutoTrackCreator')

    if not os.path.isdir(source):
        print('ERROR: AutoTrackCreator directory not found at {}'.format(source))
        sys.exit(1)

    system_path, _ = find_ableton_paths()

    if system_path is None:
        print('ERROR: Could not find Ableton MIDI Remote Scripts directory')
        print('Looked in: {}'.format(
            os.path.join(
                os.environ.get('PROGRAMDATA', r'C:\ProgramData'),
                'Ableton'
            )
        ))
        sys.exit(1)
    target = system_path

    print('Source: {}'.format(source))
    print('Target: {}'.format(target))
    print()
    install(source, target, use_symlink=args.symlink)


if __name__ == '__main__':
    main()
