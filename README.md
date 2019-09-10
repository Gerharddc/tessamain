# simonmain
Embedded touch screen 3D printer main system application as can be seen in https://youtu.be/apeYxR0ags4.

This application is designed to be the main application that runs on a 3D printer with a powerful embedded Linux SBC and a multi-touch touch screen. The application makes use of Qt and a QML interface that includes a custom keyboard.

The application includes an ultra high performance 3D rendering system for both STL files and GCode written in OpenGL. The GCode rendering system achieves superior performance to most by not trying to render meshed cylinders but instead uses shader trickery to render what are essentially lines but with the correct thickness. The shaders take viewing angles into account to create realistic lighting that appears just like cylinders.

The use interface includes the ability to place 3D models, postion them, slice them and then print the resulting GCode. The interface includes a USB drive explorer to find STL files. Furthermore the application allows for manual printer control and has automatic placing for multiple files.

The code in the slicer branch includes an ultra high performance multi-threaded C++ slicer engine that almost works perfectly. Only the final stage of the engine needs to be completed in order to produce printable outputs. This is the stage of the slicer that optimises the generated toolpaths in order to prevent the printer from randomly traveling to distant instead of neighboruing islands in a print. The slicer has been optimised for performance instead of features with the goal of being able to run smoothly on a multi core embedded device.

This project is not being maintained but might have some serious potentional should someone want to develop it a bit further.
