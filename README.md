# eurorack homogate

arduino based eurorack module to  
 - increase or decrease length of incoming gate
 - apply time offset to incoming gate
 - add or remove single gate pulses


[<img src="media/homogate-disassembled-1.jpg" height="260" />](https://github.com/othmar52/eurorack-homogate/blob/main/media/homogate-disassembled-1.jpg?raw=true)
[<img src="media/homogate-frontplate.jpg" height="260" />](https://github.com/othmar52/eurorack-homogate/blob/main/media/homogate-frontplate.jpg?raw=true)
[<img src="media/homogate-assembled-1.jpg" height="260" />](https://github.com/othmar52/eurorack-homogate/blob/main/media/homogate-assembled-1.jpg?raw=true)  

this module needs a steady (homogeneously) incoming gate/clock preferably from a clock divider.  
the "unit" homogate is dealing with is always the time interval of the incoming gate.  

the potentiometers for length and offset are 0% to 100% where 100% is the incoming interval  
In case the sum of both potentiometers exceeds 100% the offset pot gets priority and the length gets the maximum remaining time of the interval  

there are 2 instances of homogate in one 6HP module whilst the gate input from instance 1 is normalized to input instance 2  

very useful in combination with the TuringMachine (https://github.com/TomWhitwell/TuringMachine) or any other gate sequencer that does not have a reset functionality  
this module gives us the possibility to nudge a locked sequence left and right on the "timeline" by adding or supressing single gate pulses  

[![Eurorack DIY homogate](media/homogate-sheet.svg "Eurorack homogate demo")](https://github.com/othmar52/eurorack-homogate/blob/main/media/homogate-sheet.svg?raw=true)  

## parts
 - Arduino Nano Every
 - 5 x 3.5mm TS female jacks (CV clock in/out)
 - 5 x NPN Transistor N3904 (CV in/out circuit)
 - 15 x Resistor 10K (CV in/out circuit)
 - 2 x Resistor 1K (CV out circuit)
 - 3 x Diode 1N4148 (CV in circuit)
 - 4 x LED blue (in/out gate indicator)
 - 4 x Resistor 4.7K (for 2 LEDs)
 - 4 x Trimpot 10K (for gate length & offset)
 - 5 x Pushbuttons (for nudging & one future feature)
 - 1 x Box Header 10 pin or 16 pin (for connection to power BUS)
## circuit on the breadboard for one instance
[<img src="media/arduino-eurorack-diy-homogate-circuit-schematics.png" width="300" />](https://github.com/othmar52/eurorack-homogate/blob/main/media/arduino-eurorack-diy-homogate-circuit-schematics.png?raw=true)  


## diy PCB schematics
[<img src="media/homogate-pcb.png" width="300" />](https://github.com/othmar52/eurorack-homogate/blob/main/media/homogate-pcb.png?raw=true)  


module width is **6 HP**  
module depth incl. power bus cable is **38 mm**  
i measured a power consumption of **2 mA** (+12V) but i dont know if this is a reasonable value. maybe you should expect the official **19 mA** for the arduino nano instead!?  
**-12V** and **+5V** lines are not used  

[Homogate on modulargrid](https://www.modulargrid.net/e/other-unknown-homogate)



see my other [eurorack DIY projects](https://github.com/othmar52/eurorack)
