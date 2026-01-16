# Bolt Task Addon for Task Board TBv2023

This repository contains the Bolt Task Addon for Task Board version 2023 found in [https://github.com/peterso/robotlearningblock](https://github.com/peterso/robotlearningblock).

The Bolt Task requires the robot to unscrew a bolt, move it to another position and screw it again. During the operation the robot must not exceed predefined pressure force over the bolt task assembly.

Video demonstration of the first prototype and the task performed by a human can be found in the following video [https://youtu.be/7AfbINjLBa4](https://youtu.be/7AfbINjLBa4):

[![Bolt Task Addon for TBv2023 - First Prototype Demo](https://img.youtube.com/vi/7AfbINjLBa4/0.jpg)](https://www.youtube.com/watch?v=7AfbINjLBa4)

The force is measured by [M5Stack Unit Scales](https://docs.m5stack.com/en/unit/UNIT%20Scales).

The presence of the bolt is detected by two [M5Stack Unit 180](https://docs.m5stack.com/en/unit/OP180).

The used controller is [M5StickC-Plus2](https://docs.m5stack.com/en/core/M5StickC%20PLUS2). On the display visual feedback is provided. When the bolt is in the start position and at least 12 bolt revolutions are executed the top (left) part of the screen is blue. When the bolt is in the end position and at least 12 bolt revolutions are executed the bottom (right) part of the screen is green. When the pressure is near the max limit the screen will turn yellow. When the pressure is over the limit the screen will turn red. The Arduino test code of the first prototype is in the folder [./M5StickCPlus2-Arduino/BoltTaskTest/](./M5StickCPlus2-Arduino/BoltTaskTest/).

The FreeCAD ([https://www.freecad.org/](https://www.freecad.org/)) design file is [./CAD/BoltTaskAssembly_v1.FCStd](./CAD/BoltTaskAssembly_v1.FCStd). The CAD assembly of the Bolt Task Addon is shown in the following image:

![CAD Assembly](./docs/CAD%20Assembly.png)

The Bolt Task Addon can be 3D Printed. The STLs of the parts are in the folder [./STLs/](./STLs/).

The printed parts and the M5 Unit sensors are shown in the following image:

![Printed Parts and M5 Unit Sensors](./docs/parts.jpg)

The complete assembly mounted on the CU-3286-MB box is shown in the following image:

![Assembled and Mounted in the Box](./docs/placement%20in%20the%20box.jpg)

The Bolt Task Addon with the cover of the TBv2023 is shown in the following image:

![Final Assembly](./docs/assembled.jpg)





