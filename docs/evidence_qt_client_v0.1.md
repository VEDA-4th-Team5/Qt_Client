# Evidence Qt-local v0.1

This branch deliberately avoids the proposed `GET /api/v1/parking-evidence`
endpoint. The Evidence page opens as one locally sorted timeline from
`ParkingViewState::slotImages`, which has already arrived through the existing
slot and session-image APIs. During one app run, it retains the last non-empty
images observed for each slot in an Evidence-only Qt cache. Slots are filters,
not the primary navigation.

## What is processed in Qt

- Groups image variants into capture groups.
- Attaches the known slot, slot state, and plate number.
- Shows all loaded capture groups by `captured_at DESC` in the UI.
- Narrows the time-ordered result with Slot, Plate, and Reason filters.
- Displays the selected image using the existing image loader.
- Keeps already loaded images visible when a subsequent parking-status poll
  omits its `images` field.
- On entering or refreshing the All slots view, requests each currently known
  slot's existing detail/current-session image flow. Selecting one slot asks
  for that slot only.
- Repeats that request every five seconds while the Evidence page is visible;
  the timer stops when the operator leaves the page.
- Resolves the First and Latest cards only within the selected timeline row's
  exact `slot_id + session_id`; rows from another slot or session are never
  combined into one pair.
- Uses the authoritative event session ID when an event image item omits it;
  without any session ID, the UI refuses to guess a First/Latest pair.

## Deliberate boundary

It does **not** obtain historical sessions or use a new history endpoint. The
local cache is cleared when the app restarts and is not a server-side evidence
store. This v0.1 flow makes one current-session request per currently known
slot, so it can still show no result when those sessions have no evidence.
Neither version can obtain all historical sessions without a server-side history
query.
