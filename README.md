# B&K 5490C series benchtop OSD software

Development funded by Quenten Ciurro - Thankyou!

Simple Windows On-Screen-Display software for the B&K 549X series bench meters.  Great for using with OpenBroadcaster when capturing video.

Utilises hotkeys to permit you to change modes even when the OSD is not focused

This source code could be used as a template for people wanting to using it for other SCPI type meters.  Do note that even though SCPI is a "standard", each meter tends to have its own different commands and ways of doing things, so expect to have to tweak things a fair bit.

# Status

Not quite ready for general public use but it's close.  For now it's being built and adjusted on-demand as we refine the setup.  A lot of the delay in the refinement process is because it's a $1,500 USD meter and without one actually on the bench here it requires a lot of back & forth work to make new progress.




# Requirements

Currently designed to be built on a linux mingw64 enabled build system to cross compile to Windows platforms.

Depends on SDL2



# Setup

Build	 

	(linux) make
	
# Usage

    Run within the build folder, at the very least you'll need the appropriate font in the folder.

    bk5490c.exe    ( will try to auto detect the port the meter is on, assuming 9600:8n1 configuration )

    ...or...

    bk5490c.exe -p 5   ( try use COM5 )
	
# TODO

    * Flesh out support for more meter modes ( temperature, period, ACDC-Volts, AC-Current, DC-Current ) - nothing explicitly hard there, just a case of actually implementing the conversions
    * Make it work with mingw build on linux without perhaps having to explicitly defining where the mingw toolchain is in Makefile
    * Integrate the defacto font as a #include ( already have the .h/.cpp, just have to test it works )
    * Add pause mode
    * Work out how to speed up the volts measurements. By default seems to be stuck at ~2sps which is to be expected from 6.5 digit, need to determine how to tell meter to use less precision for faster results

    




