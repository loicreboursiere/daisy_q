//#include <daisy_seed.h>
#include <daisy_pod.h>
#include <q/pitch/pitch_detector.hpp>
#include <q/support/notes.hpp>
#include <q/fx/biquad.hpp>
#include <q/synth/sin.hpp>

#include "hid/wavplayer.h"


/**
 * This is a simple example program showing how the Q library can be used on the Daisy platform
 * 
 * In this example, we are simply taking the channel 1 input and feeding it into a BACF pitch 
 * detector that is configured to detect the fundamental frequency of monophonic signals 
 * between C2 (65Hz) and C5 (523Hz). When a frequency is detected, the phase accumulator
 * of a synthesized sine wave will be updated to reflect the detected frequency.
 * 
 * This synthesized sine wave is output on channel 1.
 * 
 * Channel 2 is a pass through.
 */

// Namespaces...
using namespace daisy;
namespace q = cycfi::q;
using namespace cycfi::q::literals;

//DaisySeed hardware;
DaisyPod hw;
MidiUartHandler midi;

uint32_t sample_rate = 48000;

WavPlayer player; 

// The dectected frequencies
q::frequency detected_f0 = q::frequency(0.0f);

// The frequency detection bounds;
q::frequency lowest_frequency  = q::notes::E[1];
q::frequency highest_frequency = q::notes::C[5];

// The pitch detector pre-processor
q::pd_preprocessor::config preprocessor_config;
q::pd_preprocessor         preprocessor{preprocessor_config,
                                lowest_frequency,
                                highest_frequency,
                                sample_rate};

// The pitch detector
q::pitch_detector pd{lowest_frequency, highest_frequency, sample_rate};

// A phase accumulator for our synthesized output
q::phase phase_accumulator = q::phase();
q::phase f0_phase          = q::phase(detected_f0, sample_rate);


	

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    // Iterate over the samples in our block (default block size is 48)
    for(size_t i = 0; i < size; i++)
    {
        
        // This is the input signal
        float sig_l = in[0][i];
        //float sig_r = in[1][i];
        float sig_r = player.Stream();

        // Pre-process the signal for pitch detection
        float pd_sig = preprocessor(sig_l);

        // send the processed sample through the pitch detector
        if(pd(pd_sig))
        {
            detected_f0 = q::frequency{pd.get_frequency()};
            f0_phase    = q::phase(detected_f0, sample_rate);
        }

        out[0][i] = q::sin(phase_accumulator) * 0.5;
        out[1][i] = sig_r;

        // Increment our phase accumulator
        phase_accumulator += f0_phase;
    }
}
// MIDI
// c is the channel and is something between 0-F (0-15)
// NoteOn : 0x9c NoteNb Velocity
// Example code
//      uint8_t bytes[3] = {0x90, 0x00, 0x00};
//      bytes[1] = msg.data[0];
//      bytes[2] = msg.data[1];
//      midi.SendMessage(bytes, 3);
// NoteOff : 0x8c NoteNb Velocity
// CC : 0xBc CCNb Value
// ProgramChange : 0xCc Program Number


int main(void)
{
    // Initialize the hardware
    //hw.Configure();
    hw.Init();

    // Start the audio...
    hw.StartAudio(AudioCallback);

    player.Init();
    player.Open( 0 ); // index of sound file on the sd card
    player.SetLooping( true );

    hw.StartLog();

    MidiUartHandler::Config midi_config;
    midi.Init(midi_config);

    midi.StartReceive();

    uint32_t now      = System::GetNow();
    uint32_t log_time = System::GetNow();

    /** Infinite Loop */
    while(1)
    {
        now = System::GetNow();

        /** Process MIDI in the background */
        midi.Listen();

        /** Loop through any MIDI Events */
        while(midi.HasEvents())
        {
            MidiEvent msg = midi.PopEvent();

            /** Handle messages as they come in 
             *  See DaisyExamples for some examples of this
             */
            switch(msg.type)
            {
                
                case NoteOn:
                    // Do something on Note On events
                    {
                        uint8_t bytes[3] = {0x90, 0x00, 0x00};
                        bytes[1] = msg.data[0];
                        bytes[2] = msg.data[1];
                        midi.SendMessage(bytes, 3);
                    }
                    break;
                default: break;
            }

            /** Regardless of message, let's add the message data to our queue to output */
            event_log.PushBack(msg);
        }
    
}
