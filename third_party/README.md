# Bundled dependency — standard IJG libjpeg

- Project: Independent JPEG Group libjpeg (not libjpeg-turbo)
- Official website: https://www.ijg.org/
- Current release checked: 10, released 2026-01-25 (checked 2026-11-09)
- Official source: https://www.ijg.org/files/jpegsrc.v10.tar.gz
- Downloaded archive SHA-256: `8b9eaa13242690ebd03e1728ab1edf97a81a78ed6e83624d493655f31ac95ab5`
- Import: complete, unmodified `jpeg-10` directory from the official archive.
- License and acknowledgement requirements: see `libjpeg/README`.

The wrapper supplies its own `jconfig.h` under `minimal/config`. The original
upstream source files are preserved. Build output is kept outside this directory.

This software is based in part on the work of the Independent JPEG Group.
