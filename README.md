[![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-3.0.en.html)
[![Platform](https://img.shields.io/badge/Platform-x86--64-brightgreen)](https://shields.io/)

```
  _______     ______  _                 
 | ____\ \   / / ___|| |_ ___  _ __ ___ 
 |  _|  \ \ / /\___ \| __/ _ \| '__/ _ \
 | |___  \ V /  ___) | || (_) | | |  __/
 |_____|  \_/  |____/ \__\___/|_|  \___| -- Groupability-aware caching systems for DRS

```

This repository contains the implementation code for paper:<br>
**EVSTORE: Storage and Caching Capabilities for Scaling
Embedding Tables in Deep Recommendation Systems**<br>
                        
Contact Information
--------------------

**Maintainer**: [Daniar H. Kurniawan](https://people.cs.uchicago.edu/~daniar/), Email: ``daniar@uchicago.edu``

[//]: <> (**Daniar is on the job market.** Please contact him if you have an opening for an AIOps and ML-Sys engineer role!)

Feel free to contact Daniar for any suggestions/feedback, bug
reports, or general discussions.

Please consider citing our EVStore paper at ASPLOS 2023 if you use EVStore. The bib
entry is

```
@InProceedings{Daniar-EVStore, 
Author = {Daniar H. Kurniawan and Ruipu Wang and Kahfi S. Zulkifli and Fandi A. Wiranata and John Bent and Ymir Vigfusson and Haryadi S. Gunawi},
Title = "EVSTORE: Storage and Caching Capabilities for Scaling
Embedding Tables in Deep Recommendation Systems",
Booktitle =  {Proceedings of the 28th International Conference on Architectural Support for Programming Languages and Operating Systems (ASPLOS)},
Address = {Vancouver, Canada},
Month =  {MARCH},
Year =  {2023}
}
```

Run EVStore
-----------

Please follow the experiments detailed in [Experiments.md](experiments.md).


### Acknowledgement ###

The DLRM code in this repository is based on [Facebook DLRM](https://github.com/facebookresearch/dlrm).
The cache benchmark repository is based on [Cache2k](https://github.com/cache2k/cache2k) and [Cacheus](https://github.com/sylab/cacheus/).
