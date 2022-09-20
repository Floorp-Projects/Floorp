import giturlparse

_DOT_GIT_SUFFIX = ".git"


class RepoUrlParsed(giturlparse.result.GitUrlParsed):
    @property
    def hgmo(self) -> bool:
        return self.platform == "hgmo"

    @property
    def repo_name(self) -> str:
        return self.repo_path.split("/")[-1]

    @property
    def repo_path(self) -> str:
        repo_path = (
            self.pathname[: -len(_DOT_GIT_SUFFIX)]
            if self.pathname.endswith(_DOT_GIT_SUFFIX)
            else self.pathname
        )
        return repo_path.strip("/")

    @property
    def repo_type(self) -> str:
        return "hg" if self.platform == "hgmo" else "git"

    @property
    def taskcluster_role_prefix(self) -> str:
        return f"repo:{self.host}/{self.repo_path}"
