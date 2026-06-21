
---

##  Step 1 — Open Windows Terminal

Press:

```
Win + X → Windows Terminal
```

or search *Windows Terminal* in Start.

---

##  Step 2 — Configure the serial port (same as before)

Inside Windows Terminal (PowerShell or CMD):

```powershell
mode <COM_Port>:115200,n,8,1
```

(Use your COM port in `<COM_Port>`.)

---

##  Step 3 — Monitor serial output

```powershell
type COM7
```

Windows Terminal will display ANSI escape sequences correctly (colors, formatting, etc.).

Stop with:

```
Ctrl + C
```

---

## If you want a smoother experience (recommended)

Use **PuTTY** for serial communication and Windows Terminal for everything else.

Because PuTTY gives you:

-  ✔ direct serial connection UI
- ✔ reconnect easily   
- ✔ logging
- ✔ stable streaming
- ✔ proper terminal emulation

Windows Terminal is good — PuTTY is purpose-built.

---

##  Conceptual difference

| Tool             | Role                                       |
| ---------------- | ------------------------------------------ |
| Windows Terminal | advanced text console                      |
| PuTTY            | communication terminal (serial, SSH, etc.) |

So Windows Terminal = better display
PuTTY = better serial control

---
