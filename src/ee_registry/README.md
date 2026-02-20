# Extreme event registry

All events should have at least the `name` and `required_params` key.
> [!IMPORTANT]
> The `required_params` key should match one of the entries of the `parameters` key at the root of the
extreme event plugin configuration. Anchors can be used to avoid errors and duplication.

The `enabled` key is optional, it can be set to `false` to keep an event in the configuration but not run it in the plugin.
This key is `true` by default if omitted.

- [Extreme wind](#extreme-wind)
- [Storm](#storm)
- [Extreme wave](#extreme-wave)
- [Wind drought](#wind-drought)
- [Ramp](#ramp)

## Extreme wind

### Description

This event with name `extreme_wind` detects winds above a specified threshold or within a specified range.
It scans the fields that are in the `required_params`, computes the wind magnitude from the horizontal and vertical
components (or only one of them if the other is not offered), and flags the grid points that exceed the threshold or
fall in the range. The same `extreme_wind` event can be used to detect on model levels or several thresholds, via
configuring the `instances` list key. Each element represents a set of detection options: `lower_bound`, `upper_bound`,
a human-readable `description`, and optionally, if non 2D fields are passed, `model_levels`.


> [!NOTE]
> Users can provide a `height` option in the required parameters if they are interested in detecting
> high winds at a specific height, e.g., wind turbine height. In that case, model levels cannot
> be passed to the configuration. Separate events must be configured to detect at several heights.


### Configuration examples

> [!TIP]
> Validation is run on the instances when the extreme wind object is constructed. Bad values will throw excecptions.

Things to keep in mind when writing your configuration:
- ensure your instances have the proper parameters for your field types (`model_levels` is required for 3D fields).
- ensure the vertical levels you request are not higher than the model levels.
- if you want to use a threshold and not a range, make sure to input your threshold in `lower_bound` and set the 
`upper_bound` to a smaller number.
- wind speeds are expressed in m/s.
- if you are not using anchors, ensure the `required_params` match at least one group of the `required_params` at the
root of the plugin configuration.

```yaml
parameters:
  - &extreme_wind
    - name: "u"
      type: "atlas_field"
      height: 100
    - name: "v"
      type: "atlas_field"
      height: 100
...
name: "extreme_wind"
enabled: true
required_params: *extreme_wind
instances:
  - lower_bound: 25.0 # this is a threshold
    upper_bound: 0.0
    description: "Extremely strong wind"
  - lower_bound: 0.0 # this is a range
    upper_bound: 1.0
    description: "Extremely low wind"
```

```yaml
parameters:
  - &extreme_wind
    - name: "u"
      type: "atlas_field"
    - name: "v"
      type: "atlas_field"
...
name: "extreme_wind"
required_params: *extreme_wind
instances:
  - lower_bound: 30.0
    upper_bound: 0.0
    model_levels: [1, 66, 137]
    description: "Extremely strong wind"
```

Required parameters apply to all instances, so it is not possible to use a combination of 2D and 3D 
fields, or multiple 2D fields at different heights, in the same `extreme_wind` event, e.g., correct configuration:

```yaml
parameters:
  - &extreme_wind
    - name: "u"
      type: "atlas_field"
    - name: "v"
      type: "atlas_field"
  - &100m_extreme_wind
    - name: "u"
      type: "atlas_field"
      height: 100
    - name: "v"
      type: "atlas_field"
      height: 100
...
events:
  - name: "extreme_wind"
    required_params: *extreme_wind
    instances:
      - lower_bound: 30.0
        upper_bound: 0.0
        model_levels: [1, 66, 137]
  - name: "extreme_wind"
    enabled: true
    required_params: *100m_extreme_wind
    instances:
      - lower_bound: 25.0
        upper_bound: 0.0
```

## Storm

### Description

This event with name `storm` uses a minimal definition solely based on wind speed components to serve as baseline,
but may be refined in the future. Detection is triggered when the wind (1+ grid point in the coarse cell) exceeds a
given threshold over a specified time window.

This event stores the wind speed for each grid point and each time step in the time window.

This event uses `u` and `v` fields at any given height level or model level.

### Configuration examples

Only `u` and `v` fields are allowed at the moment, as the detection method relies only on the wind.
The event requires only two keys, and has an optional one:
- The wind speed threshold: expressed in m/s, only two decimals of precision are used by the algorithm,
so anything more precise will be truncated. The algorithm will throw an error if the number provided is negative.
- The time window: expressed in minutes. If the time window is smaller than the internal model time step,
the detection will run only on the current time step.
- The model level: in case no height is provided, the event will rely on a model level being provided.

```yaml
parameters:
  - &storm
    - name: "u"
      type: "atlas_field"
    - name: "v"
      type: "atlas_field"
  - &100m_storm
    - name: "u"
      type: "atlas_field"
      height: 100
    - name: "v"
      type: "atlas_field"
      height: 100
...
name: "storm"
required_params: *storm
wind_speed_cutout: 20.0
time_window: 60
model_level: 137
...
name: "storm"
required_params: *100m_storm
wind_speed_cutout: 20.0
time_window: 60
```

## Extreme wave

### Description

This event with name `extreme_wave` detects significant wave height of combined wind sea and swell above a specified
threshold. It scans the 'swh' field, and flags the grid points that exceed the threshold(s). The same `extreme_wave`
event can be used to detect several thresholds, via configuring the `instances` list key. Each element represents a 
threhsold with its corresponding description (optional). For instance, a user can configure two thresholds: 3.5 meters
which corresponds to the height after which operations on wind farms are no longer safe, and 8 meters which suggest
stormy conditions.

> [!NOTE]
> It is assumed that the coarsening established by the plugin applies to wave fields and atmospheric fields, which is
true if they are represented on the same grid, but false otherwise.

### Configuration examples

The configuration below corresponds to the example given in the description. The thresholds are expressed in meters.
By default the missing value is set to 9999, but it can be changed from the configuration if the wave fields use a
different value. It is important to handle the missing value correctly in the wave fields because land areas are
represented by the missing value, and should not trigger the detection.

```yaml
parameters:
  - &extreme_waves
    - name: "swh"
      type: "atlas_field"
...
name: "extreme_wave"
required_params: *extreme_waves
missing_value: -9999
instances:
  - threshold: 8.0
    description: "Storm conditions"
  - threshold: 3.5
    description: "Unsafe offshore wind-farm operations"
```

## Wind drought

### Description

This event with name `wind_drought` corresponds to prolonged periods of no wind which can constitute an extreme event
from the electricity grid perspective, especially if combined with important cloud cover.

It stores for each grid point the number of time steps where the wind stayed below a given threshold. The counters
reset whenever the wind (spatial average over the coarse cell) exceeds the threshold.

This event uses `u` and `v` fields at any given height level or model level.

### Configuration examples

The event requires only two options:
- The wind speed threshold: expressed in m/s.
- The time window: expressed in minutes. The typical order of magnitude for the time window for this event is hours or
  days, but they must be converted to minutes.

If the 3D wind components are passed, this event also requires a `model_level` key.

```yaml
parameters:
  - &wind_drought
    - name: "u"
      type: "atlas_field"
      height: 100
    - name: "v"
      type: "atlas_field"
      height: 100
...
name: "wind_drought"
required_params: *wind_drought
wind_speed_cutout: 2.0
time_window: 1440
```

## Ramp

### Description

This event with name `ramp` detects if the gradient varies positiviely or negatively more than a given value.
Detection is triggered when the gradient of the parameter(s) (1+ grid point in the coarse cell) exceeds a given threshold
over a specified time window.

This event stores the parameter(s) value for each grid point and each time step in the time window.
In case multiple parameters are passed, their magnitude is computed as stored value. It lies with the user to ensure
this quantity makes sense. All parameters passed should be surface or height levels.

### Configuration examples

Any combination of fields is allowed so users must be careful when configuring. The event allows several options:
- The ramp thresholds: expressed in the same unit as the passed fields. There are two keys, one for ramp-ups and one
  for ramp-downs. At least one should be present, they both should be positive values (the algorithm handles the sign).
- The time window: expressed in minutes. If the time window is smaller than the internal model time step,
the detection will run only on the current time step.

```yaml
parameters:
  - &windRamp
    - name: "u"
      type: "atlas_field"
      height: 100
    - name: "v"
      type: "atlas_field"
      height: 100
  - &temperatureRamp
    - name: "2t"
      type: "atlas_field"
...
- name: "ramp"
  required_params: *windRamp
  ramp_up_value: 10.0
  ramp_down_value: 5.0
  time_window: 30
- name: "ramp"
  required_params: *temperatureRamp
  ramp_up_value: 10.0
  time_window: 1440
```