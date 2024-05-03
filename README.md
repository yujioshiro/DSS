# DeepSkyStacker

- [Downloading DSS](#downloading-DSS)

- [Contributing to DSS](#contributing-to-DSS)

- [Sponsoring DSS](sponsoring-DSS)

### Important Information about DeepSkyStacker

This is the github repository for all the DeepSkyStacker source code.

DeepSkyStacker is freeware created in 2006 by Luc Coiffier

It was open sourced in 2018 and is maintained by a small and dedicated team:

	David C. Partidge, Tony Cook, Mat Draper, Simon C. Smith, Vitali Pelenjow,
	Michal Schulz, Martin Toeltsch, Iustin Amihaesei

The code is open source under the terms of the BSD 3-Clause License (see LICENSE).

Copyright (c) 2006-2019, Luc Coiffier 
Copyright (c) 2018-2023, David C. Partridge, Tony Cook, Mat Draper,
					Simon C. Smith, Vitali Pelenjow, Michal Schulz,
					Martin Toeltsch, Iustin Amihaesei

## Downloading DSS
DeepSkyStacker can be downloaded through the release page at: [https://github.com/deepskystacker/DSS/releases/tag/5.1.5](https://github.com/deepskystacker/DSS/releases/tag/5.1.5). While the latest release can only be used on 64-bit versions of Windows 10 and later, older releases can be used on 32-bit machines. Download instructions and information can be found on the release page.

The software released at [http://deepskystacker.free.fr/](http://deepskystacker.free.fr/) is out of date and there are no plans to update it at this time. All releases will use this github repository at the link provided in the first paragraph. And while the software is no longer being released at this site, you can still find the manual and other helpful guides on the sidebar.

If you have any questions, comments, feature requests, or want to share your finished images, please join the forum at [https://groups.io/g/DeepSkyStacker](https://groups.io/g/DeepSkyStacker) and create a new message.
					
## Contributing to DSS

To work on the current development codebase which uses a VS solution file called DeepSkyStacker.VS2022.sln,
you will need to install Visual Studio 2022 (17.7.2).   For earlier code versions either VS2019 or VS2022 will work.

If you open DeepSkyStacker.VS2019.sln (or an earlier level) using VS2022, please **do not** accept VS2022's offer to upgrade the build environment the first time you open the
DeepSkyStacker.VS2019.sln file.

In addition to Visual Studio, you will need to install Boost 1.80.0  Qt 6.4.0, Qt 6.5.3 and Qt VS Tools 2.10.1 (rev 2), and Visual Leak Detector.

### Boost
Install Boost 1.80.0.  Once you have done that, you need to create an environment
variable called Boost_1_80_0 with the location of the Boost installation directory e.g.:
C:\Boost_1_80_0

### Qt 6.4.0, Qt 6.5.3, Qt VS Tools
Qt 6.4.0 is needed to build DeepSkyStacker 5.1.0 through 5.1.3.  Qt 6.5.1 is needed for the current development build which includes support for WIndows Themes.
In addition to you you will also need to install the Visual Studio Extension *Qt VS Tools* : (Extensions/Manage Extensions).

You will need to use *Extensions/Qt VS Tools/Qt Versions* to set up two named Qt Installations called:

*6.4.0_msvc2019_64* and<br>
*6.5.1_msvc2019_64*
	
pointing to the msvc2019_64 sub-directory of your Qt installations (for example C:\Qt\6.4.0\msvc2019_64).

### Visual Leak Detector
You should install the version of VLD that is maintained by the Microsoft Azure development team as that appears to be the only
one that is being kept up to date.
You can download the 2.5.9 of this from [Visual Leak Detector](https://github.com/Azure/vld/releases/tag/v2.5.9).
You should install it to C:\Program Files (x86)\Visual Leak Detector as that is the location that the DeepSkyStacker projects expect to find it.
In the Visual Leak Detector installation wizard, be sure to check "Add VLD directory to your environmental path".


## Sponsoring DSS
We accept financial contributions from those that would like to give. Please keep in mind this is entirely voluntary. You can read more information on sponsoring this project at [https://github.com/sponsors/deepskystacker?o=esb](https://github.com/sponsors/deepskystacker?o=esb).