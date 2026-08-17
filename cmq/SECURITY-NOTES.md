# cmq — Security Review Notes

Review of ConfigServer Mail Queues (cmq) v4.00 as published in the GPLv3
release of the ConfigServer scripts, carried out before deploying it on a
production server.

## 1. Supply chain integrity

No backdoor, dropper, beacon or other malicious code was found. Checks
performed:

- The entire `cmq/` tree is byte for byte identical to the upstream GPLv3
  release commit (`2923c43`, authored by the original ConfigServer author).
  No commit by any downstream fork owner has ever touched a file under
  `cmq/`.
- `cmq.tgz` matches the unpacked `cmq/` directory (the only difference is an
  empty `da/images` placeholder directory that the installer populates).
- The only network destinations referenced anywhere in the product are
  `configserver.com` and a FontAwesome CDN. No IP literals, no unexpected
  hosts.
- No obfuscation, encoding or dynamic evaluation: no `base64`, `eval`,
  `atob`, `fromCharCode`, `curl | sh`, reverse shells, cron edits,
  `authorized_keys` writes or account creation.
- The bundled third party libraries are unmodified upstream releases,
  confirmed by subresource integrity hash:
  - jQuery 1.12.4 — `sha256-ZosEbRLbNQzLpnKIkEdrPv7lOy9C27hHQ+Xp8a4MxAQ=`
  - Bootstrap 3.3.7 JS — `sha384-Tc5IQib027qvyjSMfHjOMaLkfuWVxZxUPnCJA7l2mCWNIpG9mGCD8wGNIcPD7Txa`
  - Bootstrap 3.3.7 CSS — `sha384-BVYiiSIFeK1dGmJRAkycuHAHRg32OmUcww7on3RYdg4Va+PmSTsz/K68vbdEjh4u`
- No precompiled binaries are shipped. The only compiled component is
  `cmq.c` (a short, readable setuid helper built at install time).
- No setuid bits are carried in the repository and there are no hidden files.

## 2. Weaknesses found and addressed

All of the issues below are long standing upstream weaknesses, not injected
code. They are fixed in this tree.

### 2.1 Unverified root level auto-update (highest risk)

`cmqUI.pm` fetched `https://<downloadserver>/cmq.tgz` and ran the bundled
`install.sh` **as root**, with no signature or checksum verification. The
download host defaulted to `download.configserver.com`, because both entries
in the shipped `downloadservers` file are commented out. ConfigServer has
ceased trading and that hostname no longer resolves. Whoever ends up
controlling the domain would have had remote root on every server running
this code, one button press away. The main page also performed an outbound
version check on every single load.

Both the upgrade action and the remote version check have been removed, along
with the now unused `urlget`, `printcmd` and `getdownloadserver` helpers.
Upgrades must be applied manually from a verified source tree.

### 2.2 Reflected cross site scripting in a root privileged page

Form input was echoed into the HTML unescaped in several places, including
the error branches that fire precisely when input contains characters like
`<`. Roughly fifteen form values were also interpolated raw into `href`
attributes, where a single quote breaks out of the attribute.

Added `esc()` (HTML entity encoding) and `urlenc()`/`formurl()` (percent
encoding), and applied them to every reflected sink.

### 2.3 Argument injection into root run exim

The mass action derived the message ID from an attacker controllable form
*field name* (`del_<id>`) and passed it straight to `exim` as a command line
argument. A value beginning with `-` is parsed by exim as an option rather
than a message ID. The same applied to `id`, the Queue Run search text and
the Bcc recipient. On DirectAdmin, where the UI is reachable by a non root
admin, this was a privilege escalation path.

Added `validid()` and `validaddress()`, and rejected leading hyphens in the
Queue Run and Exigrep search terms.

### 2.4 User supplied regular expressions

The search box interpolated user text directly into a dozen `m//` operators
evaluated against every queued message, including full message bodies. A
malformed pattern aborted the CGI and a catastrophically backtracking one
burned CPU as root. (This was not code execution: Perl refuses `(?{...})` in
an interpolated pattern without `use re 'eval'`.)

Search terms are now matched literally via `quotemeta`, which matches the
documented behaviour of the field.

### 2.5 setuid helper passed the caller's environment to a root Perl script

`cmq.c` is installed setuid root (mode 4755) and `execv`'d a `#!/usr/bin/perl`
script while inheriting the caller's environment. Perl honours `PERL5OPT` and
`PERL5LIB`, so any DirectAdmin admin passing the `admin.list` check could
load arbitrary code as root. `execv` was also called with a `NULL` argv, and
the `setuid`/`setgid` return values were unchecked.

`cmq.c` now builds a minimal allowlisted environment (`SESSION_ID`,
`SESSION_KEY`, `QUERY_STRING`, `POST`, `REQUEST_METHOD` plus a fixed `PATH`),
uses `execve` with a proper argv, and aborts if the privilege change fails.
The installer builds it with hardening flags and refuses to continue if the
compile fails.

### 2.6 Broken authentication failure path

`da_cmq.cgi` called `&loginfail(...)` on session validation failure, but that
subroutine is not defined anywhere. The script aborted with an "Undefined
subroutine" error instead of the intended message. It failed closed, so it
was not exploitable, but the path never worked as written. Replaced with a
plain message and exit.

### 2.7 Installer hygiene

`install.cpanel.sh` ran `find ./ -type f -exec sed -i ...` across the whole
working directory to rewrite shebangs, which rewrites unrelated files if run
from the wrong place. It is now scoped to the one file that needs it, and
both installers refuse to run outside the unpacked source tree. `mkdir` calls
use `-p`.

### 2.8 Third party CDN removed

The DirectAdmin UI loaded FontAwesome from `use.fontawesome.com` into a root
privileged admin page with no SRI, and none of its classes were used.
Removed.

## 3. Residual risks, accepted

- **Bundled library age.** jQuery 1.12.4 and Bootstrap 3.3.7 are from 2016
  and carry known XSS issues (jQuery CVE-2020-11022 / CVE-2020-11023;
  Bootstrap CVE-2018-14041 / CVE-2019-8331). They are only reachable through
  an injection point in the page, and the escaping above closes the ones that
  existed. Upgrading jQuery to 3.x would require replacing Bootstrap 3 as
  well, which is a larger change than this review covers.
- **No anti-CSRF token of its own.** On cPanel the pages sit behind WHM's
  session security token; on DirectAdmin behind the DA session key. Neither
  is a per-form CSRF token.
- **DirectAdmin sessions are not IP bound.** Upstream deliberately removed the
  session IP match in v3.05 (see `CHANGELOG.txt`), so a stolen DA session key
  is usable from any address.
- **`exigrep` still takes a real regular expression** by design. It runs in a
  separate process, so a bad pattern cannot abort the UI, but a pathological
  one can consume CPU against a large `mainlog`.

## 4. Deployment recommendations

- **Install from the `cmq/` directory, not from the top level `cmq.tgz`.**
  That tarball is deliberately left as the unmodified upstream artifact so
  that its integrity can still be verified against the original release; it
  does not contain the fixes in section 2.
- Install only from a source tree you have verified. Ignore any instructions
  that `wget` a tarball from `download.configserver.com`.
- Keep `/etc/cmq` at mode 700 and owned by root (the installer does this).
- Restrict access to WHM/DirectAdmin to trusted networks; the whole product
  runs as root by design and grants full control over the mail queue.
- On DirectAdmin, remember that `/usr/local/directadmin/plugins/cmq/exec/cmq`
  is setuid root. Review the `admin.list` membership that gates it.
