{
  nixConfig.bash-prompt = "[nix(cppbot):\\w]$ ";
  outputs =
    { self, nixpkgs }:
    {
      devShells.x86_64-linux.default =
        let
          pkgs = nixpkgs.legacyPackages.x86_64-linux;
        in
        import ./shell.nix {
          inherit pkgs;
        };
    };
}
