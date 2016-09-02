# Drupal Drop Ship

Uses (a fork of) [kw_manifests](github.com/promet/kw_manifests) to provide a reusable deployment composed of
manifests.

## Usage

Install like any other drupal module and run `drush kw-m`. This naturally
presupposes that you have your database and whatever other backing services
(like your cache) set up so that Drupal can bootstrap.

You can easily add any other deployment logic to your Drupal application by
implementing your own manifests, so that your deployments can go from hours long
ordeals of attempting every permutation of procedures described by the oral
history of what made it work on some machine at one time, to invoking something
as easy as this:

```bash
# while logged in to the machine of interest
cd path/to/project
# either this:
source path/to/file/with/export/statements 
drush kw-m
# or this:
DROPSHIP_SEEDS=my_sweet_module:some_other_module:etc drush kw-m
```

## What it does

Under the sheets, this algorithm will be executed

1. run update hooks by invoking `drush updb`
2. ensure ONLY the proper modules are enabled
  * This is a manifest made with code cribbed from the
    [kw-amd command](https://github.com/kraftwagen/kraftwagen/blob/master/dependencies.module.apply.kw.inc)
    in `kraftwagen`
  * It reads a list of 'seed' modules with which to build the dependency graph
    from the `DROPSHIP_SEEDS` environment variable
3. revert all features

So, there's nothing magical here about guaranteeing the simple deployments
promise above: it only works out of the box if you phrase your updates as
changes in code, using some combination of custom modules, features modules and
update hooks. Of course, feel free to implement a manifest that augments this
algorithm however you like.

## What modules exactly get enabled?

Suppose your `DROPSHIP_SEEDS` variable is set to `foo:devel` because you are
setting up your local environment to test your application, whose production
dependencies are captured by your custom module `foo` and its transitive
dependencies (the deps of its deps and so on). This will cause exactly `foo` and
`devel` and all of their dependencies to be the things enabled. This means that
if you had previously enabled some other module not in this set, it would be
disabled. Similarly, if you removed one of the dependencies from a module by
editing its info file, such that nothing that would be enabled depended on the
module named in that dependencies line, then that named module would be
disabled.

For example, suppose that I come to realize that there is no need to have
`migrate` enabled in production, though it was expedient for development and
migrating the client's legacy content. If the only module named as a dependency
of `foo` which also depends on `migrate` is something like `foo_migrate`, then
removing the line `dependencies[] = foo_migrate` from `foo.info` would cause
both `foo_migrate` and `migrate` (and anything else that depended on them) to be
disabled when we invoke `kw-m`.

This makes it simple to guarantee that the right set of modules is active when
deploying updates or building from scratch or simply concocting a manual test
scenario.

## Why `kw_manifests`?

Rather than actually write a pile of drush commands that invoke one another and
figure out option and argument passing and the Drupal bootstrap levels, it's
easier to just say what other tasks need to run before yours and run them all
with a full bootstrap and pass information to your tasks some other way. It's
also trivial to add more tasks to this system.

## Why Environment Variables?

Mainly because you need some way of passing parameters to the amd manifest and
`kw_manifests` doesn't have any facility for passing them as command line
arguments or options. We could just query the global drush context, but that's
never fun and would be unexpected behavior for a manifest like this.

Also, one could use drush site aliases, it's just that then you end up with
a question of templating those files and how to deploy them, and things just
become absurd. It's much easier to allow any shell session, scripted or
otherwise, to just set variables in the environment and execute the program.

## Roadmap

We will likely drop `kw_manifests` soon, since we would like to be able to provide
manifests that operate on the site without fully bootstrap'ing, eg for install
or for applying fixes to known broken states, like rebuilding the registry.

This project is opinionated in that it assumes you have already configured all
your backing services (like your database) and you just need to tell Drupal how
to connect to them, and that manifests are for operating on one site. As such,
we can use a composer paradigm and just load the autoloader in settings.php and
use whatever php tools we want, like `codegyre/robo` or `chh/bob` and we could
refrain from maintaining our own logic for manifest dependencies. This seems
better than cargo culting the `kw_manifests` logic if we are going to have to
implement a bootstrap-aware layer anyway.
