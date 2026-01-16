# ajrAcid

This is my fork of the venerable [MiniAcid](https://github.com/urtubia/miniacid) by urtubia. I'm not sure what will be incoroprated into MiniAcid. As long as I've got features that aren't yet available in the OG [MiniAcid](https://github.com/urtubia/miniacid), I'll provide a binary file for downloading and installing onto the CardPuter. 

One quality of life change between ajrAcid and [MiniAcid](https://github.com/urtubia/miniacid) is that when you enter a new note on a blank step on a synth pattern, it starts with the last note you entered. If you enter a new note on a blank step using S/X (octave +/-), it enters that note an octave up or down. My goal is making it as easy as possible to sequence your music quickly.

Additional sequencing features:

- G - transpose up half-step (shifts between drum voices on the drum seq)
- B - transpose down half-step  (shifts between drum voices on the drum seq)
- H - rotate pattern forward/right
- N - rotate pattern backward/left
- ' - duplicate steps 0–7 into 8–15

Swing: Currently swing is mapped onto I/O and Tab - this is temporary. I hate accidentally hitting the randomize buttons, and the new undo doesn't work with undoing randomization yet. 
- Tab: Remove swing, reset to 50/50
- I - decrease swing
- O - Increase swing

## Download
Looking for something to play with now?

ajracid.20260116.bin - includes: improved filter with less resonance; new waveforms - Juno saw, Juno PWM square, dirty analog saw; new sequencing features (transpose, rotation, row dupe); swing (controlled with I/O/Tab)

## TODO
- One of the features coming in [MiniAcid](https://github.com/urtubia/miniacid) is multiple options for drum character. 606, 808, and 909 characters are in the works. I'm planning to add my own drumkits using this new framework.
- Drum parameters (decay)

----

# MiniAcid

MiniAcid is a tiny acid groovebox for the M5Stack Cardputer. It runs two squelchy TB-303 style voices plus a punchy TR-808 inspired drum section on the Cardputer's built-in keyboard and screen, so you can noodle basslines and beats anywhere.

> Go play with it: https://miniacid.mrbook.org

## What it does
- Two independent 303 voices with filter/env controls and optional tempo-synced delay
- 16-step sequencers for both acid lines and drums, with quick randomize actions
- Live mutes for every part (two synths + eight drum lanes)
- Pattern and song arrangement system

## Using it

On the M5Stack Cardputer:

1) Flash `miniacid.ino` to your M5Stack Cardputer ADV (Arduino IDE)
2) Use the keyboard shortcuts below to play, navigate pages, and tweak sounds.  
3) Jam, tweak synths, sequence, randomize, and mute on the fly

On the web:
1) Go to https://miniacid.mrbook.org

For more detailed instructions, see the [Manual](MANUAL.md).

