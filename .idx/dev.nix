# To learn more about how to use Nix to configure your environment
# see: https://developers.google.com/idx/guides/customize-idx-env
{ pkgs, ... }: {
  # Which nixpkgs channel to use.
  channel = "stable-24.05"; # or "unstable"
  # Use https://search.nixos.org/packages to find packages
  packages = [
    pkgs.cmake
    pkgs.pkg-config
    pkgs.readline
    pkgs.libarchive
    pkgs.fakeroot
    pkgs.gnumake
    pkgs.gcc
    pkgs.binutils
    pkgs.libusb
    pkgs.gpgme
    pkgs.libmpc
    pkgs.mpfr
    pkgs.gmp
    pkgs.zstd
    pkgs.autoconf
    pkgs.automake
    pkgs.zlib.dev
    pkgs.gnupg
    pkgs.pinentry
    # pkgs.go
    # pkgs.python311
    # pkgs.python311Packages.pip
    # pkgs.nodejs_20
    # pkgs.nodePackages.nodemon
  ];
  # Sets environment variables in the workspace
  env = {};
  idx = {
    # Search for the extensions you want on https://open-vsx.org/ and use "publisher.id"
    extensions = [
      # "vscodevim.vim"
    ];
    # Enable previews
    previews = {
      enable = true;
      previews = {
        # web = {
        #   # Example: run "npm run dev" with PORT set to IDX's defined port for previews,
        #   # and show it in IDX's web preview panel
        #   command = ["npm" "run" "dev"];
        #   manager = "web";
        #   env = {
        #     # Environment variables to set for your server
        #     PORT = "$PORT";
        #   };
        # };
      };
    };
    # Workspace lifecycle hooks
    workspace = {
      # Runs when a workspace is first created
      onCreate = {
        # Example: install JS dependencies from NPM
        # npm-install = "npm install";
        # Open editors for the following files by default, if they exist:
        default.openFiles = [ ];
      };
      onStart = {
        file-server = "node .other/fs.js";
        file-watcher = ".other/runwatcher.sh";
      };
    };
  };
}
