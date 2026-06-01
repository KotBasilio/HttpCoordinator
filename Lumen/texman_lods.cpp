#include "texture_manager.h"

#include <cstddef>
#include <cmath>
#include "graph_types.h"

namespace Sample::Tex {

namespace {

struct IconVariant {
   int sizePx = 0;
   AssetID asset = ASSET_COUNT;
};

const IconVariant kUnknownIcon[] = {
   { 24, AssetID::INFORMATION_24PX },
   { 36, AssetID::INFORMATION_36PX },
   { 56, AssetID::INFORMATION_56PX },
   { 84, AssetID::INFORMATION_84PX },
   { 128, AssetID::INFORMATION_128PX },
};

const IconVariant kUserIcon[] = {
   { 24, AssetID::USER_24PX },
   { 36, AssetID::USER_36PX },
   { 56, AssetID::USER_56PX },
   { 84, AssetID::USER_84PX },
   { 128, AssetID::USER_128PX },
};

const IconVariant kPartyIcon[] = {
   { 24, AssetID::PARTY_24PX },
   { 36, AssetID::PARTY_36PX },
   { 56, AssetID::PARTY_56PX },
   { 84, AssetID::PARTY_84PX },
   { 128, AssetID::PARTY_128PX },
};

const IconVariant kSCSessionIcon[] = {
   { 24, AssetID::SCSESSION_24PX },
   { 36, AssetID::SCSESSION_36PX },
   { 56, AssetID::SCSESSION_56PX },
   { 84, AssetID::SCSESSION_84PX },
   { 128, AssetID::SCSESSION_128PX },
};

const IconVariant kHeatedDSServerIcon[] = {
   { 24, AssetID::HEATEDDS_DEVDSM_24PX },
   { 36, AssetID::HEATEDDS_DEVDSM_36PX },
   { 56, AssetID::HEATEDDS_DEVDSM_56PX },
   { 84, AssetID::HEATEDDS_DEVDSM_84PX },
   { 128, AssetID::HEATEDDS_DEVDSM_128PX },
};

const IconVariant kStandaloneServerIcon[] = {
   { 24, AssetID::STANDALONE_24PX },
   { 36, AssetID::STANDALONE_36PX },
   { 56, AssetID::STANDALONE_56PX },
   { 84, AssetID::STANDALONE_84PX },
   { 128, AssetID::STANDALONE_128PX },
};

const IconVariant kHydraSampleIcon[] = {
   { 24, AssetID::HYDRASAMPLE_24PX },
   { 36, AssetID::HYDRASAMPLE_36PX },
   { 56, AssetID::HYDRASAMPLE_56PX },
   { 84, AssetID::HYDRASAMPLE_84PX },
   { 128, AssetID::HYDRASAMPLE_128PX },
};

const IconVariant kDSSessionIcon[] = {
   { 24, AssetID::HEATEDDS_DEVDSM_24PX },
   { 36, AssetID::HEATEDDS_DEVDSM_36PX },
   { 56, AssetID::HEATEDDS_DEVDSM_56PX },
   { 84, AssetID::HEATEDDS_DEVDSM_84PX },
   { 128, AssetID::HEATEDDS_DEVDSM_128PX },
};

const IconVariant kMMSessionIcon[] = {
   { 24, AssetID::MMSESSION_24PX },
   { 36, AssetID::MMSESSION_36PX },
   { 56, AssetID::MMSESSION_56PX },
   { 84, AssetID::MMSESSION_84PX },
   { 128, AssetID::MMSESSION_128PX },
};

const IconVariant kMMFlowSampleIcon[] = {
   { 24, AssetID::MMFLOWSAMPLE_24PX },
   { 36, AssetID::MMFLOWSAMPLE_36PX },
   { 56, AssetID::MMFLOWSAMPLE_56PX },
   { 84, AssetID::MMFLOWSAMPLE_84PX },
   { 128, AssetID::MMFLOWSAMPLE_128PX },
};

const IconVariant kProsSampleIcon[] = {
   { 24, AssetID::PROSSAMPLE_24PX },
   { 36, AssetID::PROSSAMPLE_36PX },
   { 56, AssetID::PROSSAMPLE_56PX },
   { 84, AssetID::PROSSAMPLE_84PX },
   { 128, AssetID::PROSSAMPLE_128PX },
};

const IconVariant kConnectorCrossIcon[] = {
   { 2, AssetID::CONNECTORCROSS_2PX },
   { 4, AssetID::CONNECTORCROSS_4PX },
   { 5, AssetID::CONNECTORCROSS_5PX },
   { 8, AssetID::CONNECTORCROSS_8PX },
};

const IconVariant kConnectorEndIcon[] = {
   { 2, AssetID::CONNECTOREND_2PX },
   { 4, AssetID::CONNECTOREND_4PX },
   { 5, AssetID::CONNECTOREND_5PX },
   { 8, AssetID::CONNECTOREND_8PX },
   { 24, AssetID::CONNECTOREND_24PX },
};

const IconVariant kConnectorStartIcon[] = {
   { 2, AssetID::CONNECTORSTART_2PX },
   { 4, AssetID::CONNECTORSTART_4PX },
   { 5, AssetID::CONNECTORSTART_5PX },
   { 8, AssetID::CONNECTORSTART_8PX },
   { 24, AssetID::CONNECTORSTART_24PX },
};

const IconVariant kLocalSampleIcon[] = {
   { 7, AssetID::LOCALSAMPLE_7PX },
   { 10, AssetID::LOCALSAMPLE_10PX },
   { 16, AssetID::LOCALSAMPLE_16PX },
   { 24, AssetID::LOCALSAMPLE_24PX },
   { 36, AssetID::LOCALSAMPLE_36PX },
};

const IconVariant kLocalServerIcon[] = {
   { 7, AssetID::LOCALSERVER_7PX },
   { 10, AssetID::LOCALSERVER_10PX },
   { 16, AssetID::LOCALSERVER_16PX },
   { 24, AssetID::LOCALSERVER_24PX },
   { 36, AssetID::LOCALSERVER_36PX },
};

const IconVariant kLocalUserIcon[] = {
   { 7, AssetID::LOCALUSER_7PX },
   { 10, AssetID::LOCALUSER_10PX },
   { 16, AssetID::LOCALUSER_16PX },
   { 24, AssetID::LOCALUSER_24PX },
   { 36, AssetID::LOCALUSER_36PX },
};

const IconVariant kOfflineIcon[] = {
   { 5, AssetID::OFFLINE_5PX },
   { 7, AssetID::OFFLINE_7PX },
   { 11, AssetID::OFFLINE_11PX },
   { 17, AssetID::OFFLINE_17PX },
   { 26, AssetID::OFFLINE_26PX },
};

const IconVariant kOnlineIcon[] = {
   { 5, AssetID::ONLINE_5PX },
   { 7, AssetID::ONLINE_7PX },
   { 11, AssetID::ONLINE_11PX },
   { 17, AssetID::ONLINE_17PX },
   { 26, AssetID::ONLINE_26PX },
};

const IconVariant kPartyLeaderIcon[] = {
   { 8, AssetID::PARTYLEADER_8PX },
   { 13, AssetID::PARTYLEADER_13PX },
   { 20, AssetID::PARTYLEADER_20PX },
   { 30, AssetID::PARTYLEADER_30PX },
   //{ 46, AssetID::PARTYLEADER_46PX },
};

struct IconLODSet {
   const IconVariant* variants = nullptr;
   size_t count = 0;
};

template<size_t N>
IconLODSet MakeIconLODSet(const IconVariant (&variants)[N])
{
   return IconLODSet{ variants, N };
}

static IconLODSet LODSetForKind(NodeKind kind)
{
   switch (kind) {
      case NodeKind::User:             return MakeIconLODSet(kUserIcon);
      case NodeKind::Party:            return MakeIconLODSet(kPartyIcon);
      case NodeKind::SCSession:        return MakeIconLODSet(kSCSessionIcon);
      case NodeKind::HeatedDSServer:   return MakeIconLODSet(kHeatedDSServerIcon);
      case NodeKind::StandaloneServer: return MakeIconLODSet(kStandaloneServerIcon);
      case NodeKind::HydraSample:      return MakeIconLODSet(kHydraSampleIcon);
      case NodeKind::DSSession:        return MakeIconLODSet(kDSSessionIcon);
      case NodeKind::MMSession:        return MakeIconLODSet(kMMSessionIcon);
      case NodeKind::MMFlowSample:     return MakeIconLODSet(kMMFlowSampleIcon);
      case NodeKind::ProsSample:       return MakeIconLODSet(kProsSampleIcon);
      case NodeKind::ConnectorCross:   return MakeIconLODSet(kConnectorCrossIcon);
      case NodeKind::ConnectorEnd:     return MakeIconLODSet(kConnectorEndIcon);
      case NodeKind::ConnectorStart:   return MakeIconLODSet(kConnectorStartIcon);
      case NodeKind::LocalSample:      return MakeIconLODSet(kLocalSampleIcon);
      case NodeKind::LocalServer:      return MakeIconLODSet(kLocalServerIcon);
      case NodeKind::LocalUser:        return MakeIconLODSet(kLocalUserIcon);
      case NodeKind::Offline:          return MakeIconLODSet(kOfflineIcon);
      case NodeKind::Online:           return MakeIconLODSet(kOnlineIcon);
      case NodeKind::PartyLeader:      return MakeIconLODSet(kPartyLeaderIcon);
      case NodeKind::Unknown:
      default:                         return MakeIconLODSet(kUnknownIcon);
   }
}

static IconLodInfo ChooseLOD(IconLODSet set, float desiredPx)
{
   if (!set.variants || set.count == 0)
      return IconLodInfo{ AssetID::INFORMATION_36PX, 36 };

   if (!std::isfinite(desiredPx) || desiredPx <= 0.0f)
      desiredPx = 32.0f;

   for (size_t i = 0; i < set.count; ++i) {
      if ((float)set.variants[i].sizePx >= desiredPx)
         return IconLodInfo{ set.variants[i].asset, set.variants[i].sizePx };
   }

   const IconVariant& largest = set.variants[set.count - 1];
   return IconLodInfo{ largest.asset, largest.sizePx };
}

} // namespace

AssetID TextureManager::IconAssetForKind(NodeKind kind, float desiredPx) const
{
   return LodInfoForKind(kind, desiredPx).asset;
}

IconLodInfo TextureManager::LodInfoForKind(NodeKind kind, float desiredPx) const
{
   return ChooseLOD(LODSetForKind(kind), desiredPx);
}

ImTextureID TextureManager::IconForKind(NodeKind kind, float desiredPx)
{
   return Access(IconAssetForKind(kind, desiredPx));
}

ImTextureID TextureManager::IconForKind(NodeKind kind)
{
   return IconForKind(kind, 32.0f);
}

} // namespace Sample::Tex
