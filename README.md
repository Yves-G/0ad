# Description
This is my experimental OpenGL 4 branch of 0 A.D., a free, open-source, cross-platform real-time strategy game of ancient warfare. This branch aims to improve rendering performance by reducing driver overhead.
Check out these links for some information about the techniques I'm working with:

 - [Text - The Road to One Million Draws](http://www.openglsuperbible.com/2013/10/16/the-road-to-one-million-draws/)
 - [Video - Beyond Porting: How Modern OpenGL Can Radically Reduce Driver Overhead](https://www.youtube.com/watch?v=-bCeNzgiJ8I)

I'm starting with the model renderer and other rendering code is currently out of scope (terrain, UI etc.).

# Installing and testing
The instructions for compiling the game on Windows, Linux and OS X are at
http://trac.wildfiregames.com/wiki/BuildInstructions

In addition, set the following required configuration settings in your local.cfg/user.cfg:
```
gpuskinning = true
preferglsl = "true"
gentangents = "true"
```

# Known issues
 - It's currently just "hacked" to work with very specific settings and breaks other rendering modes
 - The Mesa 3D drivers on Linux only support up to OpenGL 3.3, and this only for OpenGL contexts in the core profile. At the moment you have to install proprietary drivers.
 - Many open TODOs
 
# Tasks
- [ ] Avoid uniform related state changes and validation overhead
  - [x] Use uniform blocks in the model renderer shaders
  - [x] Write code to manage uniform blocks (binding, creating buffers, storing uniform data at the right place in the buffers etc.)
  - [x] Extend material XML files and the parser code with uniform block information
  - [ ] Find a way to avoid writing uniform data based on conditional defines. Currently we write a lot of per-model data which is never used by the shader. We can't use the preprocessor defines the same way as we used them before.
- [ ] Implement drawing using glMultiDrawElementsIndirect
  - [x] Implement code to generate the draw commands, fill the buffer and draw using glMultiDrawElementsIndirect. Avoid uploading the buffer per draw.
  - [ ] Actually start making use of instancing and draw multiple objects in one draw call.
- [ ] Avoid texture related state changes and validation overhead
- [ ] Consider other improvements to avoid state changes

# Performance state
Currently the performance is slightly better in the OGL4 branch on my system (Linux, AMD R9 X270, FGLRX driver). 
Here are some test results. I'm not specifying all the details (settings, match setup details, camera perspective etc.), these results should just roughly indicate a trend.
I've treid different settings (shadows, silhouettes etc.) and there was always an improvement.

**Combat demo (huge):**
```
SVN: 13-14 FPS
OGL4 branch: 17-19 FPS 
```


**Deep forest:**
```
SVN: 19-21 FPS
OGL4 branch: 21-23 FPS
```

