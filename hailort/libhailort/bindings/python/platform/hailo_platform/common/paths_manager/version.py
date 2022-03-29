#!/usr/bin/env python

def get_version(package_name):
    # See: https://packaging.python.org/guides/single-sourcing-package-version/ (Option 5)
    # We assume that the installed package is actually the same one we import. This assumption may
    # break in some edge cases e.g. if the user modifies sys.path manually.

    # hailo_platform package has been renamed to hailort, but the import is still hailo_platform
    if package_name == "hailo_platform":
        package_name = "hailort"
    try:
        import pkg_resources
        return pkg_resources.get_distribution(package_name).version
    except:
        return 'unknown'
