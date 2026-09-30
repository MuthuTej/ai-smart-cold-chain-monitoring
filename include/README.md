# `include/`

This directory is for **public header files** that need to be shared outside of
`src/` — for example, if part of this project were later extracted into a
reusable PlatformIO library.

Right now the firmware is self-contained, so every header lives next to its
implementation inside `src/` (`src/sensors/`, `src/storage/`, and so on). The
directory is kept because PlatformIO treats it as a first-class include path,
and because a future cloud-uplink or dashboard-facing SDK would publish its
contract here.

Headers placed here are reachable as:

```cpp
#include "SomeSharedHeader.h"
```

See [`docs/architecture.md`](../docs/architecture.md) for the layer layout.
