# Evidence Qt-local v0.1

This branch deliberately avoids the proposed `GET /api/v1/parking-evidence`
endpoint. The **All loaded evidence (Qt)** control builds one locally sorted
timeline from `ParkingViewState::slotImages`, which has already arrived through
the existing slot and session-image APIs.

## What is processed in Qt

- Groups image variants into capture groups.
- Attaches the known slot, slot state, and plate number.
- Orders all loaded capture groups by `captured_at` in the UI.
- Displays the selected image using the existing image loader.

## Deliberate boundary

It does **not** obtain unrequested slots or historical sessions. An empty
timeline can therefore mean either “there is no evidence” or “this client has
not loaded evidence for that slot yet.” v0.2 addresses the first gap by asking
for each currently known slot's current session, but neither version can obtain
all historical sessions without a server-side history query.
