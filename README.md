
# chnode

## Todos

* Finish Windows support
* Ensure everything works on Linux
* Support binary directories other than `/usr/local/bin`
* Provide binaries for as many architectures as possible

## Installing

* Ensure you have `libcurl` downloaded and reachable by your system's linker
* `make install PREFIX=/parent/directory/of/your/bin/directory`

## Background

The, supposedly, best tools for this problem all seem to come with frustrating caveats.

There's `nvm`, but to use it you have to source it into your shell with something like:

```bash
[ -s "/usr/local/opt/nvm/nvm.sh" ] && . "/usr/local/opt/nvm/nvm.sh"
[ -s "/usr/local/opt/nvm/etc/bash_completion" ] && . "/usr/local/opt/nvm/etc/bash_completion"
```

Having to manually (and even automatically) perform this step is disappointing. In no way whatsoever is this better than having a `$PATH`-searchable executable for doing the job. Some computers can't do this shell set-up step quickly, either, adding noticable delay for booting new shells. It even has the side-effect of polluting the shell with loads of functions that are never used. Then there's the problem with how `nvm` updates itself. That is to say, it's outright broken in one particularly popular OS configuration. I do like how it can interpret release names like `lts/carbon`, though, and have it find the URL to the correct binary. That isn't an easy feature to provide. And how it uses rc files in projects, which is by far my favourite feature.

I tried to find other popular tools and came across `nvs`, which is actually written in JavaScript. Really missing the point there, folks. Facepalm.

So, with all of that in mind, what I'm building here:

* Only depends on `libcurl`
* Supports major operating systems
* Downloads the version of Node.js you specify (only with a version number, right now, unfortunately)
* Symlinks it to somewhere on your `$PATH`
* Keeps all the tarballs and binaries downloaded in your home directory (`~/.chnode/$version`); and
* Is just a single executable with no ahead-of-time set-up required, or complicated update flow (*cough*, `nvm`)


