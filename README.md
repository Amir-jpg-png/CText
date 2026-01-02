# CText
A text editor written in C with no dependencies.

## Features
- Text Viewing
- Text Editing
- Search Functionality
- Syntax Highlighting

## Installation and Usage

Installation:
```bash
curl -S https://raw.githubusercontent.com/Amir-jpg-png/CText/refs/heads/main/install.sh | bash
```

then follow the instructions, you are invited to look at the install script as I can understand that piping into bash without knowing what the script is trying to accomplish is rather dangerous and I would not encourage it.

Usage:

- Terminal
    - ctext -> Opens the editor
    - ctext <filename> Opens <filename> with ctext
- Editor
    - You leave the editor with Ctrl-X (keep in mind you will be prompted to escape 3 times if you have unsaved changes)
    - You save your progress with Ctrl-S
    - You enable the search function with Ctrl-F and you leave the search function
        - by pressing ESC in which case your cursor moves back to it's original position
        - by pressing Enter in which case you will land at the search result
        - You jump between matches by using the ARROW Keys

## Customization

The editor is still very raw and not developed at all this is the first version. In the future there might be ways to set options of the text editor via scripting(lua, bash) or by passing options to the editor.

For now that is not possible so the only way to configure the editor is by downloading the source code, tweaking or adding values and compiling from source.

The things you can customize are:
- Tab Stop
- Colors (Only syntax Highlighting colors)
- Key combinations (for example replace Ctrl-X with Ctrl-Q for leaving)
- Add languages and their keywords to syntax highlighting
    - For now only C gets Syntax Highlighting
    ```C
        char* C_HL_extensions[] = { ".c", ".h", ".cpp", ".hpp", NULL };
        char *C_HL_keywords[] = {
        "switch", "if", "while", "for", "break", "continue", "return", "else",
        "struct", "union", "typedef", "static", "enum", "class", "case",
        "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
        "void|", NULL
        };

        struct EditorSyntax HLDB[] = {
        {
            "c",
            C_HL_extensions,
            C_HL_keywords,
            "//",
            "/*",
            "*/",
            HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
            },
        };

    ```
    The way you add languages is by extending the highlight database

