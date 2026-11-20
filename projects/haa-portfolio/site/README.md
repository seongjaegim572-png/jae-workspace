# Jae Kim — HAA Application Portfolio

A responsive, six-page portfolio based on Jae's existing application materials and original evidence screenshots.

The static site is served from `dist/`. Its sections cover independent music, audience growth and clinic marketing, military interpretation and peer support, open questions, and Sibyl.

Sibyl is an interactive concept demo with prewritten rules. It has no connected AI, live trend ingestion, or stored visitor data. Historical metrics are labeled as snapshots; creator-reported figures are identified separately.

The deployed Site is public. Updates preserve its existing audience.

November 20, 2026 update: the homepage and Audience Growth case study now feature the Namwon Miraeyeonhap Clinic website rebuilt with Claude Code, linked at https://namwonmiraeyeonhap.com/. The brief cost comparison uses Jae's reported figures: KRW 360,000 every two years (KRW 180,000/year) versus approximately KRW 20,000/year, an approximately 89% annual reduction. The new section is available in all five supported languages.

Browser visual QA was unavailable in this environment. Local assets, cross-page anchors, metadata, and JavaScript behavior are verified before publication.

Visual identity: deep navy, cool white, and slate. The supplied smiling portrait introduces Jae on the homepage; the supplied violin portrait appears in the music card and case study. The color-assessment photos were used as design references.

Visitor preferences: every page includes a light/dark toggle and a language menu with English, Korean, Spanish, Japanese, and Simplified Chinese buttons. The theme follows the system setting until manually changed. The initial language follows the browser's supported language preference; explicit choices are stored locally and reused across pages. All portfolio text, accessibility descriptions, and Sibyl example plans are localized. Original evidence images and external platform pages retain their source language.

Translations live in `dist/assets/locales/`. `preferences.js` loads them from the same Site and handles saved choices, keyboard dismissal, and loading failures. No external translation service is required. Sibyl inputs remain local to the page.

Theme contrast: light mode uses white and pale blue surfaces with navy text, including the hero, statistics, Sibyl feature, and footer. Dark mode uses near-black navy surfaces with white text and pale blue accents.

Theme compatibility: the visible theme control contains a native checkbox, with CSS `:has()` selectors determining the palette directly from its checked state. It can switch colors without JavaScript in browsers supporting `:has()`. The independent `theme.js` adds saved choices and system preference tracking, with fallbacks for denied storage and unavailable or legacy media-query APIs. Language-loading failures do not disable the theme control.

Questions: the opening presents “What important truth do very few people agree with you on?” and “What valuable company is nobody building?” in all five supported languages. The supplied Instagram post is embedded next to these prompts using Instagram’s native embed markup and script, with a persistent direct link. Instagram content rendering depends on the post remaining embeddable and the visitor being able to load Instagram. External embed rendering was not verifiable in the development environment.

## Translation source of truth

Read `I18N.md` before changing any UI copy. The authoritative English and translated strings live in `locales/` under stable descriptive keys. Page structure lives in `src/pages/`; generated `dist/` HTML and locale files must not be edited directly. All menu labels, accessibility text, and Sibyl result templates use the same catalogs.

After edits run `node scripts/i18n.mjs build`, then `node scripts/i18n.mjs check` and the runtime/workflow checks in `tests/`. Build is dependency-free apart from Node and Python. It preserves the existing CSS, imagery, and native theme control. Stale or missing translations temporarily use English and are reported per key. The original translated strings remain available for updating.

Tolgee import/export configuration is provided as an example; no external translation service, account, key, or paid integration is active. Existing English/Korean/Spanish/Japanese/Simplified Chinese controls and saved choices remain supported.
