# Session-Level Data in PresenceSessionUpdate

Status: design proposal  
Scope: Forge / MMSession reducer + projection + Inspector  
Example source: `Hydra.Api.Push.Presence.PresenceSessionUpdate`

## Why this needs its own model

`PresenceSessionUpdate` carries useful state that belongs to the matchmaking/game session itself rather than to any individual member.

A real observed packet contains this general shape:

```json
{
  "id": {
    "id": "97509358-bc13-11f1-b593-000d3a24b8ba",
    "reason": "GAME_SESSION_ID_CHANGE_REASON_NONE"
  },
  "state": "MATCHMAKE_STATE_GAME",
  "gameData": {
    "variants": [],
    "data": {
      "data": "{...serialized JSON...}"
    },
    "membersUpdate": [],
    "sessionType": "PRESENCE_SESSION_TYPE_NONE",
    "dataCenterId": ""
  }
}
```

The confusing part is that the payload uses `data` at several layers, and two of those layers are serialized JSON strings rather than ordinary JSON objects.

Logically the observed example is:

```text
gameData
└── data                       JSON object
    └── data                   string containing JSON
        └── parsed envelope
            ├── data           string containing JSON
            │   └── parsed payload
            │       └── public
            │           ├── serverState = 2
            │           ├── serverAddress = "redstone://...:game"
            │           ├── hostName = "White Wolf"
            │           ├── region = "region_au"
            │           └── level = "UID_MAP_AU_02"
            └── rtt
                └── items[]
                    ├── EU-West = 93 ms
                    ├── SA-East = 203 ms
                    └── US-East = 171 ms
```

After decoding the two serialized JSON layers, the useful logical payload is approximately:

```json
{
  "public": {
    "serverState": 2,
    "serverAddress": "redstone://5fb0987c-b762-11f1-98c9-7ced8d0d482e:game",
    "hostName": "White Wolf",
    "region": "region_au",
    "level": "UID_MAP_AU_02"
  },
  "rtt": {
    "items": [
      { "datacenterId": "EU-West", "rttMs": 93 },
      { "datacenterId": "SA-East", "rttMs": 203 },
      { "datacenterId": "US-East", "rttMs": 171 }
    ]
  }
}
```

The current reducer already stores session identity/state, variants, session type, data-center id, settings, long-operation fields, and member state. It does not currently parse this nested `gameData.data` payload.

This information should not be placed into `SessionState::MemberInfo`: it describes the MMSession/game as a whole.

## Proposed state model

Add explicit session-level state under `SessionState`.

A small first design could look conceptually like:

```cpp
struct SessionDataState
{
   std::vector<std::pair<std::string, std::string>> publicFields;

   struct RttItem
   {
      std::string datacenterId;
      std::string rttMs;
   };

   std::vector<RttItem> rttItems;
};

struct SessionState
{
   // existing fields...
   SessionDataState data;
};
```

Names are illustrative; implementation should follow the surrounding code style.

### Why not one generic flat map?

The `public` block is naturally scalar key/value state and fits a `vector<pair<string,string>>` well. It can use the same deterministic sorted representation already used for variants and member data fields.

RTT is structurally different: it is a repeated collection of records with a stable meaning:

```text
datacenterId + rttMs
```

Flattening it immediately into keys such as `RTT_EU-West=93` would make the Inspector easy in the short term, but it would throw away the fact that these values belong to one collection of structured observations.

Keeping a small `RttItem` structure preserves that meaning and gives projection/Inspector code freedom to present it differently later.

The goal is not to create a generic arbitrary JSON tree inside `LiveState`. Store the evidence the Coordinator currently understands.

## Proposed reducer behavior

Parsing belongs in the Presence/MM reducer, following the existing architecture:

```text
SdkPacket
  -> HandleMMSessionUpdate
  -> decode session-level gameData.data
  -> SessionState
  -> projector
  -> GraphNode::kv
  -> Inspector
```

Do not parse serialized payloads in the projector or Inspector.

Recommended decoding steps:

1. Find `gameData.data`.
2. Read its inner `data` string.
3. Parse that string as a JSON object.
4. From the parsed envelope:
   - read its `data` string, if present, and parse that second serialized JSON object for the `public` section;
   - parse `rtt.items` if it is an array.
5. Store only fields whose meaning and shape are currently understood.
6. Missing, empty, malformed, or unexpected layers should be ignored safely and must not prevent the rest of `PresenceSessionUpdate` from being reduced.

The observed structure is therefore:

```text
gameData.data.data
    -> JSON string
        -> {
             data: JSON string,
             rtt: JSON object
           }
```

The implementation should follow this observed schema rather than recursively parsing every property called `data`.

## Public fields

For the observed `public` object:

```json
{
  "serverState": 2,
  "serverAddress": "redstone://...:game",
  "hostName": "White Wolf",
  "region": "region_au",
  "level": "UID_MAP_AU_02"
}
```

the first implementation should support scalar values:

- string
- integer / unsigned integer / floating point
- boolean

Nested arrays, objects, and null values can remain unsupported until real packet evidence makes them useful.

Stored public fields should be sorted by key so equality and Inspector ordering remain deterministic.

## RTT items

For the observed RTT data:

```json
[
  { "datacenterId": "EU-West", "rttMs": 93 },
  { "datacenterId": "SA-East", "rttMs": 203 },
  { "datacenterId": "US-East", "rttMs": 171 }
]
```

store each valid item as one structured record.

Suggested rules:

- require a non-empty `datacenterId`;
- accept numeric `rttMs`;
- ignore malformed individual items rather than rejecting the complete RTT section;
- preserve packet order unless later evidence shows another semantic ordering.

This is observational session data. It should not currently affect graph topology, ownership, Party/MM linking, or lifecycle decisions.

## Update semantics

The nested session data appears to represent session-state snapshots rather than member deltas.

For a valid decoded payload, treat decoded sections as the current snapshot for sections actually present:

- a valid `public` object replaces previously stored public fields;
- a valid `rtt.items` array replaces the previously stored RTT list.

If a layer is absent, malformed, or of an unexpected type, prefer retaining previously known valid state rather than erasing it based on unusable evidence.

An explicit empty object or array is a valid snapshot and clears that section:

- valid `public: {}` clears `publicFields`;
- valid `rtt.items: []` clears `rttItems`.

Absent, malformed, or unexpected layers retain previously known valid state.

## Projection and Inspector proposal

Keep `GraphNode::kv` as the current presentation surface.

For public fields, project session-level keys such as:

```text
DATA_PUBLIC_hostName       = White Wolf
DATA_PUBLIC_level          = UID_MAP_AU_02
DATA_PUBLIC_region         = region_au
DATA_PUBLIC_serverAddress  = redstone://...:game
DATA_PUBLIC_serverState    = 2
```

The exact prefix is adjustable, but it should make these values visibly distinct from member-level `MEMBER_n_DATA_*` fields.

For RTT, preserve the structured representation in `SessionState`, then flatten only at projection time if that is the smallest first UI step:

```text
RTT_0_DATACENTER_ID = EU-West
RTT_0_MS            = 93

RTT_1_DATACENTER_ID = SA-East
RTT_1_MS            = 203

RTT_2_DATACENTER_ID = US-East
RTT_2_MS            = 171
```

A later Inspector refinement may group these as an `RTT[size=N]` collection similar to the existing member grouping.

That UI refinement should not block correct reducer/state support.

## Relationship to existing fields

Do not conflate these observed values with existing MMSession fields without explicit evidence.

In particular:

- `public.region` is not automatically equivalent to `gameData.dataCenterId`.
- `public.serverAddress` is not automatically a Coordinator Server node or an SCSession relationship.
- `public.hostName` should not be interpreted as user identity merely because the observed value is `"White Wolf"`.
- `public.serverState` should remain an observed named property until the numeric enum semantics are known.

The Inspector may expose those values immediately. Graph relationships should continue to require stronger evidence.

## Non-goals for the first implementation

Do not use this work to:

- build a generic recursive JSON inspector;
- recursively parse every future object called `data`;
- infer Server/SCSession/Hydra relationships from `serverAddress`;
- reinterpret `region` as `dataCenterId`;
- redesign the entire Inspector;
- change member-level `dataFields`;
- add topology behavior based on RTT or public game data.

The first implementation should stay focused on truthful session-level storage and visibility.

## Suggested implementation slices

This is larger than the recent member-data change, so splitting it is reasonable.

### Slice A — reducer and state

- add the session-level data structure;
- decode the observed nested `gameData.data.data` envelope;
- store scalar `public` fields;
- store structured RTT items;
- add comparison/update helpers where useful.

### Slice B — projection and Inspector

- project public fields through `GraphNode::kv`;
- expose RTT values;
- initially use flat RTT keys if that keeps the change small;
- optionally add grouped RTT presentation later as a UI refinement.

The architectural boundary is more important than doing every visual refinement in one commit.

## Verification example

Given the observed packet, the MMSession Inspector should eventually expose at least:

```text
MM_SESSION_ID = 97509358-bc13-11f1-b593-000d3a24b8ba
MM_STATE = MATCHMAKE_STATE_GAME

DATA_PUBLIC_hostName = White Wolf
DATA_PUBLIC_level = UID_MAP_AU_02
DATA_PUBLIC_region = region_au
DATA_PUBLIC_serverAddress = redstone://5fb0987c-b762-11f1-98c9-7ced8d0d482e:game
DATA_PUBLIC_serverState = 2

RTT:
  EU-West = 93 ms
  SA-East = 203 ms
  US-East = 171 ms
```

Exact Inspector labels are secondary.

The important invariant is:

```text
session-level evidence
  -> session-level LiveState
  -> deterministic projection
  -> Inspector visibility
```

without turning nested SDK payload data into graph topology before the evidence justifies it.
