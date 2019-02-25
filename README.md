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

**NOTE**: You may copy the Web resource files and format.conf in `dist` directory to the place of your dictionary files. see below.

### For use with AJAX(word auto-hint) support
Copy the directory `dist/html` to the **dictionary path**(default is `/usr/share/stardict/dic`), so that all .css and .js assets are in `/usr/share/stardict/dic/html/`.

### Documentation
#### Dictionary recommendation
StarDict supports many dict formats. While sdwv is focusing on HTML.
1. xdxf(code name 'x') format is the best supported. 
2. html(code name 'h') the look and feel would depend on the dict itself.
3. text(code name 'm') is showed with basic HTML support.

#### Customized output
You can customize the output theme by editing `dist/format.conf`, and the copy it to **dictionary path**.
The format.conf is split into lines. each line makes up a single config. There are four types of line:
1. Comment. it must start with '#'.
2. Dictionary type specifier. it must start with ':', following a single char that is the same as "sametypesequence" in the .ifo file.
3. Plain string replace: x=y replaces all x to y. note there is no space in between.
4. Regular expression replace: x~y replaces any text that matches x, with y as Regex replacement string.

**NOTE**: For 3, 4: x or y can contain variable {{DICT_PATH}} as the dictionary files' absolute path, or {{DICT_NAME}} as the dictionary name.
#### Dictionaries directory layout and ordering
Actually there is only one rule for sdwv to find dictionaries in **dictionary path**: each dictionary has one unique file name but with different extentions. Using identical name in different directories will make a lot of confusion.
For the sake of ease, one directory for one dictionary. Besides the CLI -u option, the ordering depends on the full path of the dictionary file name. rename the directory of each dictionary and prefix it with _01-_, _12-_, _32-_, etc. to make them ordered. For example:
```
$ find /usr/share/stardict/dic/ | sort | grep -v res
/usr/share/stardict/dic/
/usr/share/stardict/dic/01-Dictionary-A
/usr/share/stardict/dic/01-Dictionary-A/Dictionary-A.dict.dz
/usr/share/stardict/dic/01-Dictionary-A/Dictionary-A.idx
/usr/share/stardict/dic/01-Dictionary-A/Dictionary-A.idx.oft
/usr/share/stardict/dic/01-Dictionary-A/Dictionary-A.ifo
/usr/share/stardict/dic/13-Dictionary-X
/usr/share/stardict/dic/13-Dictionary-X/Dictionary-X.dict.dz
/usr/share/stardict/dic/13-Dictionary-X/Dictionary-X.idx
/usr/share/stardict/dic/13-Dictionary-X/Dictionary-X.idx.oft
/usr/share/stardict/dic/13-Dictionary-X/Dictionary-X.ifo
/usr/share/stardict/dic/70-Dictionary-B
/usr/share/stardict/dic/70-Dictionary-B/AdictB-fix1.dict.dz
/usr/share/stardict/dic/70-Dictionary-B/AdictB-fix1.idx
/usr/share/stardict/dic/70-Dictionary-B/AdictB-fix1.idx.oft
/usr/share/stardict/dic/70-Dictionary-B/AdictB-fix1.ifo
/usr/share/stardict/dic/html
/usr/share/stardict/dic/html/autohint.js
/usr/share/stardict/dic/html/jquery.js
/usr/share/stardict/dic/html/jquery-ui.css
/usr/share/stardict/dic/html/jquery-ui.js
/usr/share/stardict/dic/format.conf
```
#### Command line help
For other helps. see `sdwv -h`.

### Bugs
If you find bug reports it via email to tomgrean at github dot com. 
Be sure to include the word "sdwv" somewhere in the "Subject:" field.

