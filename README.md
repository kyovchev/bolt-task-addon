# Bolt Task Addon for Task Board TBv2023

This repository contains the Bolt Task Addon for Task Board version 2023 found in [https://github.com/peterso/robotlearningblock](https://github.com/peterso/robotlearningblock).

The Bolt Task requires the robot to unscrew a bolt, move it to another position and screw it again. During the operation the robot must not exceed predefined pressure force over the bolt task assembly.


## Introduction

Two different protopyes of the task addon are developed:
- **Type A**
   
   The presence of the bolt is detected by two [M5Stack Unit 180](https://docs.m5stack.com/en/unit/OP180).
   
   Video demonstration of the first Type A prototype and the task performed by a human can be found in the following video [https://youtu.be/7AfbINjLBa4](https://youtu.be/7AfbINjLBa4):

   [![Bolt Task Addon for TBv2023 - First Prototype Demo](https://img.youtube.com/vi/7AfbINjLBa4/0.jpg)](https://www.youtube.com/watch?v=7AfbINjLBa4)

- **Type B**

   The presence of the bolt is detected by simple circuit connection similar to the connection of the terminal block of TBv2023 for PbHub port 3 as decribed in the original [peterso/robotlearningblock/Assembly of Task Board 2023_V2.pdf](https://drive.google.com/file/d/1LZS_wPafdJOO1Q0lu-8TDO9xGrxpoSBB/view?usp=sharing).

   The Type B design is more compact and might be used together wih the terminal block if more digital pins are available or second hub is added.

For both designs the force is measured by one [M5Stack Unit Mini Scales (SKU:U177)](https://docs.m5stack.com/en/unit/Unit-Mini%20Scales).


## Firmware

The used controller is [M5StickC-Plus2](https://docs.m5stack.com/en/core/M5StickC%20PLUS2). On the display visual feedback is provided. When the bolt is in the start position and at least 12 bolt revolutions are executed the top (left) part of the screen is blue. When the bolt is in the end position and at least 12 bolt revolutions are executed the bottom (right) part of the screen is green. When the pressure is near the max limit the screen will turn yellow. When the pressure is over the limit the screen will turn red. The Arduino test code of the first prototype for:
- **Type A** is in the folder [./M5StickCPlus2-Arduino/BoltTaskTestTypeA/](./M5StickCPlus2-Arduino/BoltTaskTest/).

- **Type B** is in the folder [./M5StickCPlus2-Arduino/BoltTaskTestTypeB/](./M5StickCPlus2-Arduino/BoltTaskTestTypeB/).


## CAD

The Bolt Task Addon can be 3D Printed. The STLs of the parts are in the folder [./STLs/](./STLs/).

The repository contains the FreeCAD ([https://www.freecad.org/](https://www.freecad.org/)) design file for the diferrent designs:

- **Type A**

   The design file is [/CAD/BoltTaskAssembly_TypeA_V1.FCStd](/CAD/BoltTaskAssembly_TypeA_V1.FCStd)

   The Type A CAD assembly of the Bolt Task Addon is shown in the following image:

   [![CAD Assembly](./docs/TypeA/CAD_Assembly_320.jpg)](./docs/TypeA/CAD_Assembly.jpg)

   The printed parts and the M5 Unit sensors are shown in the following image:

   [![Printed Parts and M5 Unit Sensors](./docs/TypeA/parts_320.jpg)](./docs/TypeA/parts.jpg)

   The complete assembly mounted on the CU-3286-MB box is shown in the following image:

   [![Assembled and Mounted in the Box](./docs/TypeA/placement_in_the_box_320.jpg)](./docs/TypeA/placement_in_the_box.jpg)

   Place two M4 nuts between the main and the top part to be used for bolt threads as shown in the image:

   [![M4 nuts](./docs/TypeA/M4_nuts_320.jpg)](./docs/TypeA/M4_nuts.jpg)

   The Bolt Task Addon with the cover of the TBv2023 is shown in the following image:
   
   [![Final Assembly](./docs/TypeA/assembled_640.jpg)](./docs/TypeA/assembled.jpg)

- **Type B**

   The design file is [/CAD/BoltTaskAssembly_TypeB_V1.FCStd](/CAD/BoltTaskAssembly_TypeB_V1.FCStd)

