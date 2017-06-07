SDEI Prototype branch
=====================
This branch implements a physical SDEI Dispatcher suitable for use in
conjunction with the physical SDEI Client implemented in [Linux]. This is a
prototype that will evolve into an upstreamable implementation. The intent is
to:

1.  Demonstrate that SDEI is suitable for addressing one of its primary use
    cases i.e. RAS error handling
2.  Gather feedback to ensure that the implementation which will be eventaully
    upstreamed fulfils partner requirements.

A description of the demonstration of RAS error handling using SDEI can be found in [sdei-demo.md].

A description of the functionality implemented by this branch and a porting
guide can be found in [sdei-implementation.md]

Any feedback should be addressed to: Achin Gupta <achin.gupta@arm.com>

- - - - - - - - - - - - - - - - - - - - - - - - - -
[Linux]:                                      http://www.linux-arm.org/git?p=linux-jm.git;a=shortlog;h=refs/heads/sdei/v1/demo
[sdei-demo.md]:                               ./docs/sdei-demo.md
[sdei-implementation.md]:                     ./docs/sdei-implementation.md
