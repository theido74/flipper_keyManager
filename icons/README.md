# App icon

`application.fam` expects `icons/keys_10px.png`:

- 10 x 10 pixels
- 1-bit (black & white), PNG
- black = lit pixel

Create one in any pixel editor (or GIMP: image 10x10, mode indexed 1-bit,
export PNG). Until you add it, you can temporarily point `fap_icon` in
`application.fam` at an icon from the firmware's `assets/icons` or remove the
`fap_icon` line to use the default.
