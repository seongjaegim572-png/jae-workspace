# Jae Kim — HAA personal website

This folder contains the editable source of Jae's published personal portfolio, including the November 20, 2026 clinic website project update.

- Live website: https://jae-kim-haa-portfolio.seong-jae-kim.chatgpt.site/
- Complete website source: [`site/`](site/)
- Prior clinic project update: [`2026-11-20-clinic-website-update.md`](2026-11-20-clinic-website-update.md)

## Start working with Claude Code

From the repository root:

```sh
cd projects/haa-portfolio/site
```

Read `README.md` and `I18N.md` in that directory. The site needs Node.js and Python 3, with a `python` command available. No package installation or API key is needed.

- Edit page structure in `src/pages/`.
- Edit copy in `locales/en.json` and its four translated catalogs. Keep existing translation keys stable.
- Edit styles and browser behavior in `dist/assets/style.css`, `preferences.js`, `site.js`, and `theme.js`.
- Images and other static assets live in `dist/assets/`.
- Do not directly edit generated HTML or generated translation files inside `dist/`; build them from the sources.

Build and validate:

```sh
node scripts/i18n.mjs build
node scripts/i18n.mjs check
node tests/i18n-runtime.test.cjs
node tests/i18n-workflow.test.mjs
```

For a local preview:

```sh
python -m http.server 4173 --directory dist
```

Then open http://localhost:4173/. Stop the preview server when finished.

## Live deployment

This is an editable GitHub source copy. A commit here does **not** automatically deploy to the existing ChatGPT Site. The live Site still has a separate Sites-managed source repository.

After Claude Code commits changes, ask ChatGPT to import the changes from `projects/haa-portfolio/site/`, validate them, and publish them to the existing Site. Preserve the project ID in `site/.openai/hosting.json` and its current audience; do not create a second Site for an update.

Example handoff:

> Update my personal portfolio using the latest changes in seongjaegim572-png/jae-workspace, projects/haa-portfolio/site/. Keep the existing ChatGPT Site URL, validate the build, and deploy it.

When the Site is edited directly through ChatGPT, copy those changes back to this folder before the next Claude editing session so the two source copies stay in sync.

## Snapshot and scope

- Exported: November 20, 2026
- Sites source commit: `51b3e1d72b10d16046b7236717e894a3660db1aa`
- Published Site version at export: 12
- Source: 48 tracked files, including all six pages and five supported languages.
- The copy contains published portfolio material. It contains no Git credentials or Gmail correspondence.
- Jae explicitly authorized including the already-public military-service portfolio page, photo, and related biography in this export. That narrow exception does not authorize adding other military information to this repository.
