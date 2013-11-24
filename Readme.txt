Movie Rotator 2 Readme. 

ABOUT 

Movie Rotator is a Windows program that re-encodes videos with a 
rotation applied. Often when users record videos on cameras and mobile 
phones, they turn their recording device sideways, and so the video 
appears the wrong way oriented when played back in some media players. 
Movie Rotator fixes this problem. 

BUILDING 

Movie Rotator can be built using the Visual Studio 2012 solution file 
found in is a Windows program. It uses Windows Media Foundation, 
Direct3D9, Direct2D, Win32, and GDI, so unfortunately it is not cross 
platform. It also uses C++11x features, so the code may not compile with 
an earlier version of Visual Studio. 

LICENSE 

Movie Rotator is open source and licensed under an Apache 2 license. 
Patches accepted. 

FAQ 

Q: Why no git history? Don't you believe in version control? 
A: I was intending to sell licenses for Movie Rotator 2, so I had code 
in my private repo for licensing/activation etc, which I can't 
distribute. 

Q: Selling software is evil. Software should be free! Why you want sell 
software?!?! 
A: It was hard work building this, and it was hard finding the time to 
build this. And have you seen the price of real estate lately? 

Q: Why did you decide not to sell Movie Rotator? 
A: I ended up not selling Movie Rotator 2 since I realised demand was 
waning. Back when I started working on Movie Rotator 2 in Jan 2013, my 
monthly number of downloads of Movie Rotator 1 from MovieRotator.com was 
holding steady, but since then downloads have been steadily declining. I 
assume people started to mostly record and watch videos on their mobile 
phones and tablets, which handle rotation, or more desktop media players 
started to handle the appropriate bits in the MP4 file format that 
signal rotation. So I figured I may as well open source this, so someone 
may learn something from it. 

Q: Why did you target Windows only?
A: I wanted to learn more about Windows Media Foundation. 

Q: Why didn't you just use ffmpeg/gstreamer/libwhatever to re-encode 
with rotation, rather than using Windows Media Foundation? Then your 
software could have been cross platform. 
A: Because of software patents. If I use WMF then I don't need to ship 
H.264/AAC codecs, and Microsoft pays the patent license for the codecs I 
use. 

Q: Why is the code *so* bad? 
A: I had severely limited time to code this in (I have a young family), 
and my design and knowledge of C++11x evolved as I worked on it, so once 
I got it working I never bothered spending much time cleaning it up. 

Q: Why didn't you write an MFT to do the rotation and use the 
MediaSession or Transcode API, instead of using SinkWriter and 
SourceReader? 
A: Yeah. If I wrote this again, that's probably how I'd do it. 

Q: Would you accept a patch to do ___?
A: Maybe! Ask me. :) 

