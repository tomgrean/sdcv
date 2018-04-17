# How to compile and install
### Compile
```
mkdir /tmp/build-sdwv
cd /tmp/build-sdwv
cmake path/to/source/code/of/sdwv
make
```
### Install
```
make install
```
you can use "DESTDIR" variable to change installation path

### Documentation
See sdwv man page for usage description.

### Bugs
If you find bug reports it via email to tomgrean at github dot com. 
Be sure to include the word "sdwv" somewhere in the "Subject:" field.

### For use with AJAX support
Copy the directory `dist/html` to the **dictionary path**(default is
`/usr/share/stardict/dic`), so that all .css and .js assets are in
`/usr/share/stardict/dic/html/`.
