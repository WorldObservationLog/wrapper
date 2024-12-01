# wrapper
A tool to decrypt Apple Music's music. An active subscription is still needed.
Only support Linux x86_64 and arm64.

# Install
Get the pre-built version from this project's Actions. Or you can refer to the Actions configuration file for compilation.

# Usage
```
Usage: wrapper [OPTION]...

  -h, --help              Print help and exit
  -V, --version           Print version and exit
  -H, --host=STRING         (default=`127.0.0.1')
  -D, --decrypt-port=INT    (default=`10020')
  -M, --m3u8-port=INT       (default=`20020')
  -P, --proxy=STRING        (default=`')
  -L, --login=STRING        (username:password)
```

# Special thanks
- Anonymous, for providing the original version of this project and the legacy Frida decryption method.
- chocomint, for providing support for arm64 arch.