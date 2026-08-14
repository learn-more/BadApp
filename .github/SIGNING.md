# Code signing

Release binaries are Authenticode signed with a Certum code signing certificate, through
[learn-more/certum-signing](https://github.com/learn-more/certum-signing). How that works - the cloud HSM, the
SimplySign session, the TOTP login, where to get each value - is documented there, in
[SIGNING.md](https://github.com/learn-more/certum-signing/blob/main/SIGNING.md). This file is only what is
specific to BadApp.

**Signing is optional.** Without the settings below a release still builds; it just comes out unsigned and says
so, in an annotation, the job summary, and the release notes. Nothing has to be configured to cut a release.

Only [`release.yml`](workflows/release.yml) signs, and it only runs on a tag push. `msbuild.yml` does not, on
purpose: pull request builds from forks cannot see secrets anyway, and every signature costs a login to a
certificate that is meant for artifacts people actually download.

## What to store

**Settings → Secrets and variables → Actions → Secrets**

| Secret | What it is |
| --- | --- |
| `CERTUM_USERNAME` | The SimplySign account name. |
| `CERTUM_OTP_URI` | The whole `otpauth://totp/...` URI from SimplySign enrolment. |

**Settings → Secrets and variables → Actions → Variables**

| Variable | Required | Default |
| --- | --- | --- |
| `CERTUM_CERTIFICATE_SHA1` | yes | - |
| `CERTUM_TIMESTAMP_SERVER` | no | `http://time.certum.pl` |
| `CERTUM_INTERMEDIATE_URL` | no | `https://repository.certum.pl/ccsca2021.cer` |

All three of the first are required together; missing any one turns signing off for the whole workflow. The
thumbprint is a variable rather than a secret because a certificate thumbprint is public information, and a wrong
one is much easier to spot in a log than a masked `***`.

These are stored per repository. Renewing the certificate changes `CERTUM_CERTIFICATE_SHA1` here and in every
other repository that signs.

## What gets signed, and in what order

Two files: `bin\BadApp-x86.exe` and `bin\BadApp-x64.exe`. The PDBs ship alongside them but carry no signature,
and nothing is embedded in either executable, so the ordering is the simple one - build everything, then open the
SimplySign session, then sign. That keeps the session as short as it can be: one TOTP code has to cover the
signing only, not the build in front of it.

1. **Build** both platforms into `bin\`.
2. **Sign and report** - `certum-signing@v1`, which connects, signs, and then reads the result back off the
   files. The run fails here if signing was configured and did not take.
3. **Package** each executable with its PDB into `dist\BadApp-x86.zip` and `dist\BadApp-x64.zip`, after signing,
   because what a user runs is whatever came out of the zip.
4. **Hash** the archives into `SHA256SUMS.txt`.

`signtool /d` sets the program name in the UAC and SmartScreen dialogs. The workflow passes no `description:`,
unlike JobDebug: `BadApp.exe` has a version resource, and the action prefers the `FileDescription` in it -
*"Bad Application"*, from [`BadApp/BadApp.rc2`](../BadApp/BadApp.rc2) - so the string stays in a single place.

## One login per build

A login spends a one-time code, so there must be exactly one per build. Two inside the same 30-second TOTP step
present the *same* code twice, which Certum may refuse - surfacing as a release that fails to sign for no visible
reason, on some builds and not others.

Hence `release.yml` builds x86 and x64 in **one job**, rather than the `matrix: arch: [x86, x64]` that
`msbuild.yml` uses. Restoring the matrix here means solving the login first - serialising it, or signing in a
single job the others feed. The cost of one job: a compile error no longer names its platform without reading
the log, and the platforms no longer build in parallel.

## Testing the login

The TOTP generator can be checked against the RFC 6238 vectors without an account, an install, or a certificate
store. Clone [certum-signing](https://github.com/learn-more/certum-signing) and run:

```bash
pwsh -File scripts/connect-simplysign.ps1 -SelfTest
```
