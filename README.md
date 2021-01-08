### LDAC decoder

this is an early stage, jet functional LDAC audio stream decoder.

Shout-out to @Thealexbarney for the heavy lifting.
LDAC is basically a stripped down, streaming only ATRAC9.

#### Build
```sh
$ make
```

#### Usage
see ldacdec.c for example usage

#### ldacdec
takes an LDAC stream and decodes it to WAV

#### ldacenc
uses Android LDAC encoder library to create LDAC streams from audio
