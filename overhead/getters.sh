#!/usr/bin/env bash

./scatter_all.R \
  1418.116 ../usecases/keepassxc/protected/experiment/no_protection/b.out.obf_bbls \
  "`echo -e "KeepassXC P1 (0.057\u03BCs/OP - 12% executed)"`" 0.00005670 `# api.xml: S1(+S0)` \
  "`echo -e "KeepassXC P2 (5.993\u03BCs/OP - 12% executed)"`" 0.00599303 `# api.xml: G0` \
  "" \
  16.80477 ../usecases/ninja/protected/experiment/no_protection/b.out.obf_bbls \
  "`echo -e "Ninja P1 (0.370\u03BCs/OP - 35% executed)"`" 0.00036954  `# api.xml: P2` \
  "`echo -e "Ninja P2 (0.613\u03BCs/OP - 35% executed)"`" 0.00061318  `# api.xml: P3` \
  `#Ninja P3 (0.0557\u03BCs/OP - 35% executed)" 0.00005573` `# api.xml: P0` \
  "" \
  23703.88 ../usecases/python/protected/experiment/no_protection/b.out.obf_bbls \
  "`echo -e "Python P1 (0.069\u03BCs/OP - 26% executed)"`" 0.00006851 `# api.xml: P0` \
  "`echo -e "Python P2 (0.150\u03BCs/OP - 26% executed)"`" 0.00015041 `# api.xml: P1` \
  "" \
  122790.992056 ../usecases/radare2/protected/experiment/no_protection/b.out.obf_bbls \
  "`echo -e "Radare2 P1 (0.136\u03BCs/OP - 14% executed)"`" 0.00013585 `# api.xml: P0` \
  "`echo -e "Radare2 P2 (0.175\u03BCs/OP - 14% executed)"`" 0.00017491 `# api.xml: P2` \
  `#"Radare2 P3 (0.284\u03BCs/OP - 14% executed)" 0.00028442` `# api.xml: P1`
