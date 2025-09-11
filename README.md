# Nonverbal Communication Device (Morse Code Assistive System)

## 📌 Overview
This project is a Morse code–inspired communication device designed for nonverbal patients (e.g., ALS).  
Users enter short dot–dash sequences using a button, which are mapped to words or phrases and displayed on an LCD screen.  
It can also be used as a memory game to help patients practice and recall patterns.

## ✨ Features
- Button input for dot–dash Morse-like sequences  
- Lookup table maps sequences to key phrases (e.g., `.-` = "Yes")  
- Real-time message display on LCD screen  
- Simple design for accessibility and memory support  

## ⚙️ How It Works
1. Patient presses the button in short (dot) or long (dash) intervals.  
2. Arduino processes the input and matches it to a phrase from the lookup table.  
3. The interpreted message is shown on an LCD for caregivers/family.  

## 🛠️ Parts & Tools
- 2 Arduino Unos (or compatible board)  
- LCD Display
- Potentiometer 
- Push buttons  
- Resistors and jumper wires  
- Breadboard
- Piezo buzzer/speaker
- LEDs
- Diodes

## 🚀 Future Improvements
- Expand phrase library with more sequences  
- Add audio or haptic feedback for accessibility  
- Create a wireless/mobile app integration  

