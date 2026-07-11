from __future__ import absolute_import, print_function, unicode_literals

import os
import json
import Live
from _Framework.ControlSurface import ControlSurface


DEFERRED_DELAY_TICKS = 10


class AutoTrackCreator(ControlSurface):

    def __init__(self, c_instance):
        super(AutoTrackCreator, self).__init__(c_instance)
        self._config = None
        self._created_track = None
        with self.component_guard():
            self.log_message('AutoTrackCreator: Initializing...')
            self._config = self._load_config()
            self._create_track()
            self.schedule_message(
                DEFERRED_DELAY_TICKS, self._deferred_setup
            )
            self.log_message('AutoTrackCreator: Initialization complete.')

    def _load_config(self):
        try:
            config_path = os.path.join(
                os.path.dirname(os.path.realpath(__file__)), 'config.json'
            )
            with open(config_path, 'r') as f:
                config = json.load(f)
            self.log_message(
                'AutoTrackCreator: Config loaded from {}'.format(config_path)
            )
            return config
        except Exception as e:
            self.log_message(
                'AutoTrackCreator: Could not load config.json ({}), '
                'using defaults'.format(e)
            )
            return {
                'track_name': 'Auto-MIDI',
                'midi_input': '',
                'midi_input_channel': '',
                'midi_output': '',
                'midi_output_channel': '',
            }

    def _create_track(self):
        try:
            song = self.song()
            new_index = len(song.tracks)
            song.create_midi_track(new_index)
            self._created_track = song.tracks[new_index]

            self._created_track.name = self._config.get(
                'track_name', 'Auto-MIDI'
            )
            self.log_message(
                'AutoTrackCreator: Created MIDI track "{}" at index {}'.format(
                    self._created_track.name, new_index
                )
            )

        except Live.Base.LimitationError:
            self.log_message(
                'AutoTrackCreator: ERROR - Track limit reached '
                '(Live Lite: max 8 MIDI + 8 Audio tracks)'
            )
        except Exception as e:
            self.log_message(
                'AutoTrackCreator: ERROR creating track: {}'.format(e)
            )

    def _deferred_setup(self):
        if self._created_track is None:
            self.log_message(
                'AutoTrackCreator: No track to configure (skipping).'
            )
            return
        try:
            self._log_available_routing(self._created_track)
            self._apply_routing(self._created_track, self._config)
        except Exception as e:
            self.log_message(
                'AutoTrackCreator: ERROR in deferred setup: {}'.format(e)
            )

    def _log_available_routing(self, track):
        self.log_message(
            'AutoTrackCreator: === Available MIDI Input Devices ==='
        )
        for rt in track.available_input_routing_types:
            self.log_message('  Input type: "{}"'.format(rt.display_name))
        self.log_message('AutoTrackCreator: --- Input Channels ---')
        for ch in track.available_input_routing_channels:
            self.log_message('  Input channel: "{}"'.format(ch.display_name))

        self.log_message(
            'AutoTrackCreator: === Available MIDI Output Devices ==='
        )
        for rt in track.available_output_routing_types:
            self.log_message('  Output type: "{}"'.format(rt.display_name))
        self.log_message('AutoTrackCreator: --- Output Channels ---')
        for ch in track.available_output_routing_channels:
            self.log_message('  Output channel: "{}"'.format(ch.display_name))

    def _apply_routing(self, track, config):
        midi_input = config.get('midi_input', '')
        midi_input_channel = config.get('midi_input_channel', '')
        midi_output = config.get('midi_output', '')
        midi_output_channel = config.get('midi_output_channel', '')

        if midi_input:
            self._set_routing(
                track, 'input_routing_type',
                'available_input_routing_types', midi_input
            )
        if midi_input_channel:
            self._set_routing(
                track, 'input_routing_channel',
                'available_input_routing_channels', midi_input_channel
            )
        if midi_output:
            self._set_routing(
                track, 'output_routing_type',
                'available_output_routing_types', midi_output
            )
        if midi_output_channel:
            self._set_routing(
                track, 'output_routing_channel',
                'available_output_routing_channels', midi_output_channel
            )

    def _set_routing(self, track, property_name, available_property,
                     target_name):
        available = getattr(track, available_property)
        for routing in available:
            if routing.display_name == target_name:
                setattr(track, property_name, routing)
                self.log_message(
                    'AutoTrackCreator: Set {} to "{}"'.format(
                        property_name, target_name
                    )
                )
                return
        self.log_message(
            'AutoTrackCreator: WARNING - "{}" not found for {}. '
            'Check Log.txt for available options.'.format(
                target_name, property_name
            )
        )

    def disconnect(self):
        self.log_message('AutoTrackCreator: Disconnecting...')
        super(AutoTrackCreator, self).disconnect()
