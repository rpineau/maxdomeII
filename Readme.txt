This is the first attempt at a X2 plugin to interface with  Diffraction Limited
MaxDome II controller. Most of the code comes from the INDI driver written by
Ferran Casarramona ( https://sourceforge.net/p/indi/code/HEAD/tree/trunk/3rdparty/indi-maxdomeii/ ).

This is beta code..
The dome rotation part was tested (connect/disconnect, find home, park/unpark and goto) but not the shutter code.
Due to the way the controller work it is recommended to set the pulling rate in TheSkyX Pro to 300 ms (Dome update interval in imaging setup -> dome).


Rodolphe

