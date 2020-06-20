# neresex
Resource extractor for Windows 3.xx 16-bit New Executable (NE) files.

Use on Windows 3.xx era .DLL and .EXE files.

## Credits
* NE header struct from: [https://wiki.osdev.org/NE](https://wiki.osdev.org/NE)
* Helpful NE format info: [http://bytepointer.com/resources/win16_ne_exe_format_win3.0.htm](http://bytepointer.com/resources/win16_ne_exe_format_win3.0.htm)

## License
GNU General Public License version 2 or any later version (GPL-2.0-or-later).

## Usage
```
neresex inputFile -dump prefix -usenames

  inputFile               a NE file. the only required parameter.
  -dump prefix            dumps the files out with the specified prefix.
                          e.g. -dump output_folder/
  -usenames               when dumping, use resource names as filenames.
```
