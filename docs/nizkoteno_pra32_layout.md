# Buttons

pad_1, pad_2, pad_3, pad_4,
pad_5, pad_6, pad_7, pad_8,
         pad_9, pad_10

# Piano Mode

c, d, e, f,
g, a, b, oct~ (Green=Mid, Red=High, Blue=Low),
         f1/Chord (Pink when active), f2/Params (Blue)

f1 + f2 = back to SEQ mode

C + D = C#
D + E = D#
F + G = F#
G + A = G#
A + B = A#

f1 chord mode
f2 params

## Chords

Hold f1 and select 1 of 8 chords (pads 1-8):

- X (X Major) — Major triad (3 notes).

- Xm (X Minor) — Minor triad (3 notes).

- X5 (X Power Chord) — Fifth, often used in rock (2–3 notes).

- Xmaj7 (Major 7th Chord) — Major with major 7th (4 notes).

- X7 (Dominant 7th Chord) — Major with minor 7th (4 notes).

- Xm7 (Minor 7th Chord) — Minor with minor 7th (4 notes).

- Xdim (Diminished Triad) — Characterized by tense sound (3 notes).

- Off - Disable chords

After releasing f1, return to piano mode; now pressing any note builds the chord. For example, C will build into Cmaj7.

# Params Mode

## Select Params

osc (cyan), seq (yellow), filt (green), eg (orange),
lfo (blue),  amp (red),   fx (magenta), misc (amber),
                          piano (pink),  seq (page color),       

- **Pads 1-8**: Select parameter group (LEDs show colors as listed).
- **Pad 9 (f1)**: Switch to **PIANO** mode.
- **Pad 10 (f2)**: Switch to **SEQ** mode.

#### Adjusting Parameters
When a parameter is changed, pads 1-4 act as a white **Progress Bar** for 2 seconds, showing the current value.
In a group sub-menu:
- **Pad 9**: Back to Select Params menu (Red).
- **Pad 10**: Toggle sub-page (Grey/White).

### OSC

#### osc_page1

osc1.wave+, osc1.shape+, osc1.morph+,mix.noise_sub+,
osc1.wave-, osc1.shape-, osc1.morph-,mix.noise_sub-,
                         back,       page

#### osc_page2

osc2.wave+, osc2.coarse+, osc2.pitch+,mix.osc1_2+,
osc2.wave-, osc2.coarse-, osc2.pitch-,mix.osc1_2+,
                          back,       page

---

### SEQ

#### seq_page1

seq.bpm+,seq.pbSpeed+,seq.gateLength+, seq.swing+,
seq.bpm-,seq.pbSpeed-,seq.gateLength+, seq.swing+,
                                   back,       page,


---

### FILT

#### filt_page1

filt.cutoff+, filt.res+, eg.filtAMT+, lfo.filtAMT+
filt.cutoff-, filt.res-, eg.filtAMT-, lfo.filtAMT-
                         back,        page


#### filt_page2

filt.keytrk+, filt.mode+, breath.filtAMT+,     x,
filt.keytrk-, filt.mode-, breath.filtAMT-,     x,
                         back,  page

---

### EG

#### eg_page1

eg.attk+, eg.decay+, eg.sus+, eg.rel+
eg.attk-, eg.decay-, eg.sus-, eg.rel-
                     back,    page

"eg.sus+" + "eg.rel+" > eg.rel=decay on/off
"eg.sus-" + "eg.rel-" > eg.rel=decay on/off

#### eg_page2

eg.VelSens+, eg.oscDst~,eg.oscAMT+, eg.filtAMT+,
eg.VelSens-, eg.ampMod~,eg.oscAMT-, eg.filtAMT-,
                        back,       page

---

### AMP

#### amp_page1

amp.attk+, amp.decay+, amp.sus+, amp.rel+
amp.attk-, amp.decay-, amp.sus-, amp.rel-
                       back,     page

#### amp_page2

amp.VelSens+,amp.gain+,breath.ampMod~, x,
amp.VelSens-,amp.gain-,x,              x,
                                       back, page

---

### LFO

#### lfo_page1

lfo.wave+, lfo.rate+, lfo.depth+,  lfo.fadeTime+,
lfo.wave-, lfo.rate-, lfo.depth-,  lfo.fadeTime-,
                                   back,          page

#### lfo_page2

x, lfo.oscDst~,lfo.oscAMT+, lfo.filtAMT+,
x, lfo.oscDst~,lfo.oscAMT+, lfo.filtAMT+,
                            back,        page

----

### FX

#### fx_page1

delay.fb+, delay.time+, delay.mode~, x,
delay.fb-, delay.time-, x,           x,
                                     back, page

#### fx_page2

chorus.rate+, chorus.depth+, chorus.mix+, x
chorus.rate-, chorus.depth-, chorus.mix-, x
                                          back, page

---
### MISC
#### misc_page1

portamento+, voice.mode~, pitchBend.range+,      preset+, 
portamento-, voice.assignMode~,pitchBend.range-, preset-,
                                                 back, page,


---

# SEQ Mode

## Step View (SEQ)

st1, st2, st3, st4,
st5, st6, st7, st8
               param,  page

param+st* > enter edit step
page+st* > stop step (sequence resets to step 1 before reaching this step)
st* > mute step
param+page > play/pause
f1+f2+st* > short press for load, long press for save 


---

## Step Edit (Note)

c, d, e, f,
g, a, b, oct~ (Green=Mid, Red=High, Blue=Low),
        done (red), page (slot color)

- **done (9)**: Back to step view.
- **page (10)**: Switch note slot (1-4). LED 10 shows slot color (Cyan, Yellow, Green, Orange). RED when chord is used for this step or only 1 voice is available.

c + d = c#
d + e = d#
f + g = f#
g + a = g#
a + b = a#

done + pad1-pad8 > select chord for step
