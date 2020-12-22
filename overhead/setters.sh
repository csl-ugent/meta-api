#!/usr/bin/env bash

./scatter_all.R \
  1418.116 ../usecases/keepassxc/protected/experiment/no_protection/b.out.obf_bbls \
  "`echo -e "KeepassXC P1 (0.411\u03BCs/OP - 12% executed)"`" 0.0004115  `# api.xml: S1(+S0)` \
  "`echo -e "KeepassXC P2 (68.314\u03BCs/OP - 12% executed)"`" 0.0683143 `# api.xml: G0` \
  "" \
  16.80477 ../usecases/ninja/protected/experiment/no_protection/b.out.obf_bbls \
  "`echo -e "Ninja P1 (1.509\u03BCs/OP - 35% executed)"`" 0.0015090 `# api.xml: P2` \
  "`echo -e "Ninja P2 (0.650\u03BCs/OP - 35% executed)"`" 0.0006504 `# api.xml: P3` \
  `#"Ninja P3 (0.799\u03BCs/OP - 35% executed)" 0.0007987` `# api.xml: P0` \
  "" \
  23703.88 ../usecases/python/protected/experiment/no_protection/b.out.obf_bbls \
  "`echo -e "Python P1 (0.577\u03BCs/OP - 26% executed)"`" 0.0005767 `# api.xml: P0` \
  "`echo -e "Python P2 (1.537\u03BCs/OP - 26% executed)"`" 0.0015365 `# api.xml: P1` \
  "" \
  122790.992056 ../usecases/radare2/protected/experiment/no_protection/b.out.obf_bbls \
  "`echo -e "Radare2 P1 (1.661\u03BCs/OP - 14% executed)"`" 0.0016606 `# api.xml: P0` \
  "`echo -e "Radare2 P2 (0.247\u03BCs/OP - 14% executed)"`" 0.0002466 `# api.xml: P2` \
  `#"Radare2 P3 (1.516\u03BCs/OP - 14% executed)" 0.0015162` `# api.xml: P1`
